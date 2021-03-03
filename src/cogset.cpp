#include "cogset.h"

#include "colors.h"
#include "rang.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <time.h>

#define PADDING 27

using namespace std;

Cogset& Cogset::operator=(const Cogset& other) {
    cogs = other.cogs;
    return *this;
}

void Cogset::load() {
    // remove dead cogs
    cogs.erase(remove_if(cogs.begin(), cogs.end(), [](Cog& c) { return c.getHP() == 0; }), cogs.end());
}

void Cogset::load(queue<Cog>& q) {
    // remove dead cogs
    cogs.erase(remove_if(cogs.begin(), cogs.end(), [](Cog& c) { return c.getHP() == 0; }), cogs.end());
    if (cogs.size() != 4) {
        // attempt to replace dead cogs
        while (!q.empty() && cogs.size() < 4) {
            cogs.insert(cogs.begin(), q.front());
            q.pop();
        }
    }
}

void Cogset::update() {
    if (roundUpdate) {
        for (Cog& c : cogs) {
            c.update();
        }
    }
}

const Cog& Cogset::getCog(int pos) const {
    if (pos >= (int)cogs.size()) {
        throw invalid_argument("Out of bounds");
    }
    return cogs.at(pos);
}

Cog& Cogset::getCog(int pos) {
    if (pos >= (int)cogs.size()) {
        throw invalid_argument("Out of bounds");
    }
    return cogs[pos];
}

bool Cogset::allDead() const {
    bool allDead = true;
    for (size_t i = 0; (i < cogs.size() && allDead); ++i) {
        if (cogs[i].getHP() != 0) {
            allDead = false;
        }
    }
    return allDead;
}

/*
Handle attacks and printing of damages
*/
void Cogset::attack(const vector<int>& affected, char type) {
    int PREPADDING = 1; // squirt round digit
    bool activity = false;
    for (int i : affected) {
        if (i) {
            activity = true;
            break;
        }
    }
    if (activity) {
        if (printCogset) {
            cout << "\t" << string(PREPADDING, ' ');
        }
        if (affected.size() == cogs.size()) {
            for (size_t i = 0; i < cogs.size(); ++i) {
                if (affected[i]) {
                    cogs[i].hit(affected[i]);
                    if (printCogset) {
                        switch (type) {
                        case 0:
                            cout << DAMAGE;
                            break;
                        case 1:
                            cout << KNOCKBACK;
                            break;
                        case 2:
                            cout << COMBO;
                            break;
                        default:
                            break;
                        }
                        cout << affected[i] << rang::style::reset << string(PADDING - (int)log10(affected[i]) - 1, ' ');
                    }
                } else {
                    if (printCogset) {
                        cout << string(PADDING, ' ');
                    }
                }
            }
        }
        if (printCogset) {
            cout << endl;
        }
    }
}

void Cogset::attack(const vector<int>& affected) { attack(affected, 0); }

int Cogset::numLured() const {
    int lurecount = 0;
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (cogs[i].getLured()) {
            ++lurecount;
        }
    }
    return lurecount;
}

void Cogset::gagCheck(const Gag& gagchoice) const {
    if (gagchoice.target < -2 || gagchoice.target >= (int)cogs.size()) {
        throw invalid_argument("Out of bounds");
    }
    size_t trapcount = 0;
    size_t lurecount = numLured();
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (cogs[i].getTrap()) {
            ++trapcount;
        }
    }
    if (gagchoice.kind == GagKind::TRAP) {
        if (cogs[gagchoice.target].getTrap()) {
                throw invalid_argument("Cannot trap an already trapped cog");
        } else if (cogs[gagchoice.target].getLured()) {
            throw invalid_argument("Cannot trap an already lured cog");
        }
    } else if (gagchoice.kind == GagKind::LURE) {
        if (gagchoice.target == -1 && lurecount == cogs.size()) {
            throw invalid_argument("Cannot group lure when all cogs are already lured");
        } else if (gagchoice.target != -1 && cogs[gagchoice.target].getLured()) {
            throw invalid_argument("Cannot lure an already lured cog");
        }
    }
}

void Cogset::fireTurn(const vector<Gag>& fires) {
    vector<int> fired(cogs.size(), 0);
    for (const Gag& g : fires) {
        fired[g.target] = cogs[g.target].getHP();
    }
    // temporarily turn off printing for fires
    bool currentPrintConf = printCogset;
    printCogset = false;
    attack(fired);
    printCogset = currentPrintConf;
    if (printCogset) {
        cout << "Fire" << rang::style::reset << "\t";
        print(fired);
    }
}

void Cogset::trapTurn(const vector<Gag>& traps) {
    vector<int> setups(cogs.size(), 0);
    for (const Gag& g : traps) {
        if (cogs[g.target].getHP() != 0) {
            if (setups[g.target] || cogs[g.target].getTrap() || cogs[g.target].getLured()) {
                // trap conflict - all traps targeting this cog become null
                setups[g.target] = -1;
            } else {
                setups[g.target] = g.prestiged ? g.damage + cogs[g.target].getLevel() * 3 : g.damage;
            }
        }
    }
    // set traps
    for (int& i : setups) {
        if (i == -1) {
            i = 0;
        }
    }
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (setups[i]) {
            cogs[i].setTrap(setups[i]);
        }
    }
    if (printCogset) {
        cout << TRAPPED << "Trap" << rang::style::reset << "\t";
        print(setups);
    }
}

void Cogset::lureTurn(const vector<Gag>& lures) {
    vector<int> damages(cogs.size(), 0);
    vector<int> lured(cogs.size(), 0);
    // lure rounds do not stack, and prestige overrides non-prestige, but not the other way around
    // it is assumed that max buff is shared among all toons
    int bonusKnockback = 0;
    for (const Gag& g : lures) {
        bonusKnockback = g.bonusEffect > bonusKnockback ? g.bonusEffect : bonusKnockback;
        if (g.target == -1) {
            // group lure
            for (size_t i = 0; i < cogs.size(); ++i) {
                if (cogs[i].getHP() != 0 && !cogs[i].getLured() && abs(lured[i]) < g.passiveEffect) {
                    if (g.prestiged) {
                        // this gag dominates
                        lured[i] = g.passiveEffect;
                    } else {
                        // this gag's duration dominates, but is not prestiged
                        lured[i] = lured[i] > 0 ? g.passiveEffect : -1 * g.passiveEffect;
                    }
                    if (cogs[i].getTrap()) {
                        damages[i] = cogs[i].getTrap();
                    }
                }
            }
        } else if (cogs[g.target].getHP() != 0 && !cogs[g.target].getLured() && abs(lured[g.target]) < g.passiveEffect) {
            if (g.prestiged) {
                // this gag dominates
                lured[g.target] = g.passiveEffect;
            } else {
                // this gag's duration dominates, but is not prestiged
                lured[g.target] = lured[g.target] > 0 ? g.passiveEffect : -1 * g.passiveEffect;
            }
            if (cogs[g.target].getTrap()) {
                damages[g.target] = cogs[g.target].getTrap();
            }
        }
    }
    // apply lure
    attack(damages);
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (lured[i]) {
            cogs[i].setLured(abs(lured[i]), lured[i] > 0, bonusKnockback);
        }
    }
    if (printCogset) {
        cout << LURED << "Lure" << rang::style::reset << "\t";
        print(lured);
    }
}

void Cogset::soundTurn(const vector<Gag>& sounds) {
    // find max
    int maxlvl = -1;
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (cogs[i].getLevel() > maxlvl) {
            maxlvl = cogs[i].getLevel();
        }
    }
    // get damages
    int damage = 0;
    for (const Gag& g : sounds) {
        // add raw damage
        damage += g.damage;
        // apply prestige bonus
        if (g.prestiged) {
            damage += (maxlvl + 1) / 2;
        }
    }

    // damage and print effect
    vector<int> affected(cogs.size(), damage);
    attack(affected);

    // apply multiple gag bonus
    if (sounds.size() > 1) {
        attack(vector<int>(cogs.size(), ceil(damage * 0.2)), 2);
    }

    if (printCogset) {
        cout << "Sound\t";
        print(affected);
    }
}

void Cogset::squirtTurn(const vector<Gag>& squirts) {
    vector<int> damages(cogs.size(), 0);
    vector<bool> multi(cogs.size(), false);
    vector<int> soakRounds(cogs.size(), 0);
    vector<bool> presSoaked(cogs.size(), false);
    // soak rounds do not stack but can be reset to a higher number
    // same mechanic applies to surrounding cogs for prestige squirt
    for (const Gag& g : squirts) {
        int effectiveRounds = g.passiveEffect;
        if (cogs[g.target].getHP() != 0) {
            if (damages[g.target]) {
                multi[g.target] = true;
            }
            // accumulate raw damage
            damages[g.target] += g.damage;
            // handle soaking
            if (g.prestiged) {
                if (g.target - 1 >= 0 && cogs[g.target - 1].getHP() != 0
                    && effectiveRounds > soakRounds[g.target - 1]) {
                    soakRounds[g.target - 1] = effectiveRounds;
                    presSoaked[g.target - 1] = true;
                }
                if (g.target + 1 < (int)cogs.size() && cogs[g.target + 1].getHP() != 0
                    && effectiveRounds > soakRounds[g.target + 1]) {
                    soakRounds[g.target + 1] = effectiveRounds;
                    presSoaked[g.target + 1] = true;
                }
            }
            if (effectiveRounds > soakRounds[g.target]) {
                soakRounds[g.target] = effectiveRounds;
                presSoaked[g.target] = true;
            }
        }
    }
    for (size_t i = 0; i < soakRounds.size(); ++i) {
        if (!presSoaked[i]) {
            soakRounds[i] = -1 * soakRounds[i];
        }
    }
    // damage and print effect
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (soakRounds[i]) {
            cogs[i].setSoaked(abs(soakRounds[i]));
        }
    }
    vector<int> knockback(cogs.size(), 0);
    vector<int> combo(cogs.size(), 0);
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (damages[i]) {
            if (cogs[i].getLured()) {
                // lure knockback
                knockback[i] = ceil(damages[i] * cogs[i].getLuredKnockback() / 100.0);
            }
            if (multi[i]) {
                // combo bonus
                combo[i] = ceil(damages[i] * 0.2);
            }
        }
    }
    attack(damages);
    attack(knockback, 1);
    attack(combo, 2);
    if (printCogset) {
        cout << SOAKED << "Squirt" << rang::style::reset << "\t";
        print(damages);
    }
}

void Cogset::zapTurn(const vector<Gag>& zaps) {
    vector<vector<int>> allDamages;
    vector<int> sumDamages(cogs.size(), 0);
    // keep track of cogs that have been jumped in the same turn
    vector<bool> jumped(cogs.size(), false);
    // obtain soaked cogs valid for zapping
    vector<bool> soaked;
    vector<int> zaC(cogs.size(), 0);
    for (size_t i = 0; i < cogs.size(); ++i) {
        soaked.push_back(cogs[i].getSoaked());
    }
    for (const Gag& g : zaps) {
        vector<int> damages(cogs.size(), 0);
        if (!soaked[g.target] || cogs[g.target].getHP() == 0) {
            // zap doesn't travel - starting on a dry/dead cog
            // if dry, still apply damage
            if (cogs[g.target].getHP()) {
                damages[g.target] += g.damage;
                zaC[g.target]++;
            }
        } else {
            // examine each zap's effect on all cogs (avoid recalculating on the same cog)
            vector<bool> examined(cogs.size(), false);
            size_t hitCount = 0, examineCount = 0;
            int targ = g.target, lastTarget = g.target;
            char dir = -1;
            float dropoff = g.prestiged ? 0.5 : 0.75;
            while (hitCount < 3 && examineCount < cogs.size() && abs(targ - lastTarget) <= 2) {
                // keep checking until jump count limit reached, all cogs examined, or zap fails to jump
                if (!examined[targ]) {
                    if (hitCount == 0) {
                        // at this point, first cog is already determined to be zappable
                        damages[targ] += ceil((3 - hitCount * dropoff) * g.damage);
                        zaC[targ]++;
                        ++hitCount;
                    } else if (!jumped[targ] && soaked[targ] && (cogs[targ].getHP() - sumDamages[targ] > 0)) {
                        // cog is zappable - not jumped, soaked, and living after previous zaps
                        damages[targ] += ceil((3 - hitCount * dropoff) * g.damage);
                        zaC[targ]++;
                        jumped[targ] = true;
                        lastTarget = targ;
                        ++hitCount;
                    }
                    examined[targ] = true;
                    ++examineCount;
                }
                // change direction if necessary
                if (targ == 0) {
                    dir = 1;
                } else if (targ == (int)(cogs.size() - 1)) {
                    dir = -1;
                }
                targ += dir;
            }
        }
        allDamages.push_back(damages);
        for (size_t i = 0; i < damages.size(); ++i) {
            sumDamages[i] += damages[i];
        }
    }
    // damage and print effect
    for (const vector<int>& d : allDamages) {
        attack(d);
    }
    // reduce soak
    for (size_t i = 0; i < cogs.size(); ++i) {
        cogs[i].reduceSoaked(zaC[i]);
    }
    if (printCogset) {
        cout << ZAPPED << "Zap" << rang::style::reset << "\t";
        print(sumDamages);
    }
}

void Cogset::throwTurn(const vector<Gag>& throws) {
    vector<int> damages(cogs.size(), 0);
    vector<bool> multi(cogs.size(), false);
    for (const Gag& g : throws) {
        if (cogs[g.target].getHP() != 0) {
            if (damages[g.target]) {
                multi[g.target] = true;
            }
            // accumulate raw damage
            damages[g.target] += g.prestiged ? ceil(g.damage * 1.15) : g.damage;
        }
    }
    // damage and print effect
    vector<int> knockback(cogs.size(), 0);
    vector<int> combo(cogs.size(), 0);
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (damages[i]) {
            if (cogs[i].getLured()) {
                // lure knockback
                knockback[i] = ceil(damages[i] * cogs[i].getLuredKnockback() / 100.0);
            }
            if (multi[i]) {
                // combo bonus
                combo[i] = ceil(damages[i] * 0.2);
            }
        }
    }
    attack(damages);
    attack(knockback, 1);
    attack(combo, 2);
    if (printCogset) {
        cout << THROWN << "Throw" << rang::style::reset << "\t";
        print(damages);
    }
}

void Cogset::dropTurn(const vector<Gag>& drops) {
    vector<int> damages(cogs.size(), 0);
    vector<int> multi(cogs.size(), 0);
    // rip rain combos
    for (const Gag& g : drops) {
        if (cogs[g.target].getHP() != 0 && !cogs[g.target].getLured()) {
            // accumulate raw damage
            damages[g.target] += g.damage;
            if (g.prestiged) {
                multi[g.target] += 15;
            } else {
                multi[g.target] += 10;
            }
        }
    }
    // calculate combo
    vector<int> combo(cogs.size(), 0);
    for (size_t i = 0; i < cogs.size(); ++i) {
        if (damages[i]) {
            if (multi[i] > 15) {
                // combo bonus
                combo[i] = ceil(damages[i] * (multi[i] + 10) / 100.0);
            }
        }
    }
    attack(damages);
    attack(combo, 2);
    if (printCogset) {
        cout << DROPPED << "Drop" << rang::style::reset << "\t";
        print(damages);
    }
}

ostream& operator<<(ostream& out, const Cogset& cogset) {
    for (const Cog& cog : cogset.cogs) {
        out << cog << string(PADDING - cog.getPrintSize(), ' ');
    }
    return out;
}

void Cogset::print(const vector<int>& affected) const {
    if (printCogset && affected.size() == cogs.size()) {
        for (size_t i = 0; i < cogs.size(); ++i) {
            if (affected[i]) {
                cout << cogs[i] << ATTACKED << string(PADDING - 1 - cogs[i].getPrintSize(), ' ');
            } else {
                cout << cogs[i] << string(PADDING - cogs[i].getPrintSize(), ' ');
            }
        }
        cout << endl;
    }
}
