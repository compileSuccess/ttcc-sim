#include "battle.h"

#include "colors.h"
#include "rang.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <time.h>

using namespace std;

Battle::Battle() {  // default test run Battle
    srand(time(NULL));
    gc = GagCollection::read(file_path);
    queue<int> set;
    size_t min_level = 7;
    size_t max_level = 15;
    if (rand() % 2 == 0) {
        set.push(rand() % (min_level / 2) + min_level);
    }
    set.push(max_level);
    for (size_t repeat = 0; repeat < 3; ++repeat) {
        for (size_t i = min_level; i < max_level; ++i) {
            int random = rand() % 4;
            if (random > 0) {
                set.push(i);
            }
            if (random > 1) {
                set.push(i);
            }
        }
    }
    c = Cogset(set);
    position_definition["right"] = c.getSize() - 1;
}

Battle::Battle(queue<int> set) : c(Cogset(set)) {  // pre generated set
    gc = GagCollection::read(file_path);
    position_definition["right"] = c.getSize() - 1;
}

Battle::Battle(vector<Cog> set) : c(Cogset(set)) {  // pre generated set
    gc = GagCollection::read(file_path);
    position_definition["right"] = c.getSize() - 1;
}

void Battle::turn(Battle::Strategy strat) {
    vector<vector<Gag>> gags;
    size_t num_gagtracks = 8;
    for (size_t i = 0; i < num_gagtracks + 1; ++i) {
        vector<Gag> gagtrack;
        gags.push_back(gagtrack);
    }
    for (size_t i = 0; i < strat.gags.size(); ++i) {
        if (auto_pres) {
            strat.gags[i].prestiged = true;
        }
        if (strat.gags[i].kind != GagKind::PASS) {
            gags[(int)strat.gags[i].kind].push_back(strat.gags[i]);
        }
    }
    // sort accordingly
    for (size_t i = 0; i < gags.size(); ++i) {
        vector<Gag>& gagtrack = gags[i];
        if (i == 5) {
            if (strat.config == 0) {
                sort(gagtrack.begin(), gagtrack.end(), GagComparator());
            } else if (strat.config == 1) {
                sort(gagtrack.begin(), gagtrack.end(), CrossGagComparator());
            } else if (strat.config == 2) {
                sort(gagtrack.begin(), gagtrack.end(), OrderedGagComparator());
            }
        } else {
            sort(gagtrack.begin(), gagtrack.end(), GagComparator());
        }
    }
    // handle fires first
    if (!gags[num_gagtracks].empty()) {
        fire_turn(gags[num_gagtracks]);
    }
    for (size_t i = 0; i < num_gagtracks; ++i) {
        if (!gags[i].empty()) {
            switch (i) {
            case 1:
                trap_turn(gags[i]);
                break;
            case 2:
                lure_turn(gags[i]);
                break;
            case 3:
                sound_turn(gags[i]);
                break;
            case 4:
                squirt_turn(gags[i]);
                break;
            case 5:
                zap_turn(gags[i]);
                break;
            case 6:
                throw_turn(gags[i]);
                break;
            case 7:
                drop_turn(gags[i]);
                break;
            default:
                break;
            }
        }
        bool alldead = true;
        for (size_t i = 0; i < c.getSize(); ++i) {
            if (c.getCog(i).getHP() != 0) {
                alldead = false;
                break;
            }
        }
        if (alldead) {
            break;
        }
    }
    c.load();
    position_definition["right"] = c.getSize() - 1;
}

Battle::Strategy Battle::parse_oneliner(string strat) {
    vector<Gag> gags;
    vector<int> director;
    stringstream ss(strat);
    string parser;
    size_t config = 0;
    // each loop parses a gag+target or a multi zap combo
    while (ss >> parser) {
        if (parser == "SKIP" || parser == "DELETE" || parser == "FIREALL") {
            for (size_t i = 0; i < c.getSize(); ++i) {
                c.getCog(i).hit(c.getCog(i).getHP());
            }
            break;
        }
        int multiplier = 1;
        size_t quickhand = 0;
        bool pres = false;
        if (!gc.contains(parser) && parser != "FIRE") {  // strategy first (mult gags or quickhand)
            if (parser.size() == 1 && isdigit(parser[0])) {  // multiple gag
                multiplier = stoi(parser);
                ss >> parser;
                // check if followed with gag or pres
                if (!gc.contains(parser) && parser != "pres") {
                    throw invalid_argument("Please supply the respective gag following your strategy");
                }
            }
            if (parser == "pres") {
                pres = true;
                ss >> parser;
            }
            if (parser.size() == c.getSize()) {  // might be a quickhand strategy callout in x and -
                for (size_t i = 0; i < c.getSize(); ++i) {
                    if (parser[i] == 'x') {
                        director.push_back(i);
                        ++quickhand;
                    } else if (parser[i] == 'X') {
                        director.push_back(i);
                        director.push_back(i);
                        quickhand += 2;
                    } else if (parser[i] != '-') {
                        if (!gc.contains(parser)) {
                            throw invalid_argument("Unrecognized quickhand strategy");
                        }
                        break;
                    }
                }
                // check if followed with gag, pres, or cross
                if (quickhand && !(ss >> parser)) {
                    throw invalid_argument("Missing arguments after quickhand strategy");
                }
                if (!gc.contains(parser) && parser != "cross" && parser != "pres" && parser != "FIRE") {
                    throw invalid_argument("Wrong arguments after the quickhand strategy");
                }
                if (parser == "cross") {
                    config = 1;
                    ss >> parser;
                }
            } else if (!gc.contains(parser) && parser != "FIRE") {
                throw invalid_argument("Unrecognized gag/command");
            }
        }
        // parsed should now specify a gag
        if (quickhand) {  // quickhand combo
            for (size_t i = 0; i < quickhand; ++i) {
                bool t_pres = false;
                if (parser == "FIRE") {
                    Gag fire;
                    fire.kind = GagKind::FIRE;
                    gags.push_back(fire);
                } else {
                    if (parser == "pres") {
                        t_pres = true;
                        ss >> parser;
                    }
                    if (gc.contains(parser)) {
                        Gag gagchoice = gc.get(parser);
                        gagchoice.prestiged = pres || t_pres;
                        gags.push_back(gagchoice);
                    } else {
                        throw invalid_argument("Unrecognized gag following quickhand strat");
                    }
                }
                if (i != quickhand - 1) {
                    ss >> parser;
                }
            }
            pres = false;
        } else {  // parse gag, then the target following it
            if (parser == "pres") {
                pres = true;
                ss >> parser;
            }
            Gag gagchoice;
            if (parser != "FIRE") {
                gagchoice = gc.get(parser);
            } else {
                gagchoice.kind = GagKind::FIRE;
            }
            gagchoice.prestiged = pres;

            if ((gagchoice.kind == GagKind::LURE && gagchoice.name.find("dollar") == string::npos)
                || gagchoice.kind == GagKind::SOUND) {
                for (int i = 0; i < multiplier; ++i) {
                    director.push_back(-1);  // placeholder for multi hit (lure/sound)
                }
            } else if (gc.isSOS(parser)) {
                // sos
                director.push_back(-1);
            } else if (c.getSize() == 1) {
                // single target, automatic
                director.push_back(0);
            } else {
                ss >> parser;
                if (parser[0] == '!') {
                    try {
                        int target = stoi(parser.substr(parser.find("!") + 1));
                        if (target < 0 || target >= (int)c.getSize()) {
                            throw invalid_argument("out of bounds");
                        }
                        director.push_back(target);
                    } catch (const invalid_argument& e) {
                        throw invalid_argument("Invalid index for " + gagchoice.name);
                    }
                } else if (position_definition.find(parser) != position_definition.end()) {  // word position check
                    if (position_definition[parser] >= (int)c.getSize()) {
                        throw invalid_argument("Invalid position for " + gagchoice.name);
                    }
                    for (int i = 0; i < multiplier; ++i) {
                        director.push_back(position_definition[parser]);
                    }
                } else {  // cog level position check
                    bool directed = false;
                    for (size_t i = 0; i < c.getSize(); i++) {
                        if (c.getCog(i).getLevelName() == parser) {
                            for (int j = 0; j < multiplier; ++j) {
                                director.push_back(i);
                            }
                            directed = true;
                            break;
                        }
                    }
                    if (!directed) {
                        throw invalid_argument("Position not supplied for " + gagchoice.name);
                    }
                }
            }
            for (int i = 0; i < multiplier; ++i) {
                gags.push_back(gagchoice);
            }
        }
    }
    // check for invalid gag usage & use fires
    size_t trapcount = 0;
    size_t lurecount = 0;
    for (size_t i = 0; i < c.getSize(); ++i) {
        if (c.getCog(i).getTrap()) {
            ++trapcount;
        }
        if (c.getCog(i).getLured()) {
            ++lurecount;
        }
    }
    for (size_t i = 0; i < gags.size(); ++i) {
        if (gags[i].kind == GagKind::TRAP) {
            if (director[i] == -1) {
                // note: the checks below deviate from the true game experience, which is buggy with trap SOS
                if (trapcount == c.getSize()) {
                    throw invalid_argument("Cannot place an SOS trap when all cogs are already trapped");
                }
                if (lurecount == c.getSize()) {
                    throw invalid_argument("Cannot place an SOS trap when all cogs are already lured");
                }
            } else {
                if (c.getCog(director[i]).getTrap()) {
                    throw invalid_argument("Cannot trap an already trapped cog");
                }
                if (c.getCog(director[i]).getLured()) {
                    throw invalid_argument("Cannot trap an already lured cog");
                }
            }
        } else if (gags[i].kind == GagKind::LURE) {
            if (director[i] == -1 && lurecount == c.getSize()) {
                throw invalid_argument("Cannot group lure when all cogs are already lured");
            } else if (director[i] != -1 && c.getCog(director[i]).getLured()) {
                throw invalid_argument("Cannot lure an already lured cog");
            }
        }
        gags[i].target = director[i];
    }
    Battle::Strategy strategy;
    strategy.gags = gags;
    strategy.config = config;
    return strategy;
}

Battle::Strategy Battle::parse_gags() {
    vector<Gag> gags;
    // invalid gag usage check
    size_t trapcount = 0;
    size_t lurecount = 0;
    for (size_t i = 0; i < c.getSize(); ++i) {
        if (c.getCog(i).getTrap()) {
            ++trapcount;
        }
        if (c.getCog(i).getLured()) {
            ++lurecount;
        }
    }
    size_t numtoons = 4;
    for (size_t i = 1; i <= numtoons; ++i) {
        Gag gagchoice;
        string gag_name;
        string target;
        bool pr = false;
        cout << PROMPT << "Toon " << i << ": " << rang::style::reset;
        cin >> gag_name;
        if (gag_name == "pres") {
            pr = true;
            cin >> gag_name;
        }
        while (!gc.contains(gag_name) && gag_name != "PASS" && gag_name != "FIRE") {
            cerr << "Unrecognized gag!" << endl;
            cin.ignore();
            cout << PROMPT << "Toon " << i << ": " << rang::style::reset;
            cin >> gag_name;
            if (gag_name == "pres") {
                pr = true;
                cin >> gag_name;
            }
        }
        if (gag_name != "PASS") {
            if (gag_name != "FIRE") {
                gagchoice = gc.get(gag_name);
            } else {
                gagchoice.kind = GagKind::FIRE;
            }
            if ((gagchoice.kind == GagKind::LURE && gagchoice.name.find("dollar") == string::npos)
                || gagchoice.kind == GagKind::SOUND || gc.isSOS(gag_name)) {
                gagchoice.target = -1;
            } else if (c.getSize() == 1) {
                // single target, automatic
                gagchoice.target = 0;
            } else {
                // parse target
                cin >> target;
                if (target[0] == '!') {
                    try {
                        gagchoice.target = stoi(target.substr(target.find("!") + 1));
                        if (gagchoice.target < 0 || gagchoice.target >= (int)c.getSize())
                            throw invalid_argument("out of bounds");
                    } catch (const invalid_argument& e) {
                        cerr << "Invalid index" << endl;
                        --i;
                        continue;
                    }
                } else if (position_definition.find(target) != position_definition.end()) {
                    if (position_definition[target] >= (int)c.getSize()) {
                        cerr << "Invalid position" << endl;
                        --i;
                        continue;
                    }
                    gagchoice.target = position_definition[target];
                } else {
                    bool directed = false;
                    for (size_t i = 0; i < c.getSize(); i++) {
                        if (c.getCog(i).getLevelName() == target) {
                            gagchoice.target = i;
                            directed = true;
                            break;
                        }
                    }
                    if (!directed) {
                        cerr << "Unrecognized target" << endl;
                        --i;
                        continue;
                    }
                }
            }
        } else {
            gagchoice.target = -1;
        }
        // check for invalid gag usage
        bool usage_error = false;
        if (gagchoice.kind == GagKind::TRAP) {
            if (gagchoice.target == -1) {
                // note: the checks below deviate from the true game experience, which is buggy with trap SOS
                if (trapcount == c.getSize()) {
                    cerr << "Cannot place an SOS trap when all cogs are already trapped" << endl;
                    usage_error = true;
                } else if (lurecount == c.getSize()) {
                    cerr << "Cannot place an SOS trap when all cogs are already lured" << endl;
                    usage_error = true;
                }
            } else {
                if (c.getCog(gagchoice.target).getTrap()) {
                    cerr << "Cannot trap an already trapped cog" << endl;
                    usage_error = true;
                } else if (c.getCog(gagchoice.target).getLured()) {
                    cerr << "Cannot trap an already lured cog" << endl;
                    usage_error = true;
                }
            }
        } else if (gagchoice.kind == GagKind::LURE) {
            if (gagchoice.target == -1 && lurecount == c.getSize()) {
                cerr << "Cannot group lure when all cogs are already lured" << endl;
                usage_error = true;
            } else if (gagchoice.target != -1 && c.getCog(gagchoice.target).getLured()) {
                cerr << "Cannot lure an already lured cog" << endl;
                usage_error = true;
            }
        }
        if (usage_error) {
            --i;
            continue;
        }
        gagchoice.prestiged = pr;
        gags.push_back(gagchoice);
    }
    cin.ignore();
    Battle::Strategy strategy;
    strategy.gags = gags;
    strategy.config = 2;
    return strategy;
}

void Battle::fire_turn(vector<Gag> fires) {
    vector<int> fired(c.getSize(), 0);
    for (Gag g : fires) {
        c.getCog(g.target).hit(c.getCog(g.target).getHP());
        fired[g.target] = 1;
    }
    cout << "Fire" << rang::style::reset << "\t";
    c.print(fired);
}

void Battle::trap_turn(vector<Gag> traps) {
    vector<int> setups(c.getSize(), 0);
    for (Gag g : traps) {
        if (g.target == -1) {
            // sos gag - hits all
            for (size_t i = 0; i < setups.size(); ++i) {
                if (setups[i]) {
                    // trap conflict - all traps become null
                    for (size_t i = 0; i < setups.size(); ++i) {
                        setups[i] = -1;
                    }
                    break;
                } else if (c.getCog(i).getTrap() || c.getCog(i).getLured()) {
                    setups[i] = -1;
                } else if (c.getCog(i).getHP() != 0) {
                    setups[i] = g.prestiged ? g.damage + c.getCog(i).getLevel() * 3 : g.damage;
                }
            }
        } else if (c.getCog(g.target).getHP() != 0) {
            if (setups[g.target] || c.getCog(g.target).getTrap() || c.getCog(g.target).getLured()) {
                // trap conflict - all traps targeting this cog become null
                setups[g.target] = -1;
            } else {
                setups[g.target] = g.prestiged ? g.damage + c.getCog(g.target).getLevel() * 3 : g.damage;
            }
        }
    }
    // set traps
    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        if (setups[i] > 0) {
            cog.setTrap(setups[i]);
        }
    }
    cout << TRAPPED << "Trap" << rang::style::reset << "\t";
    c.print(setups);
}

void Battle::lure_turn(vector<Gag> lures) {
    vector<int> lured(c.getSize(), 0);
    for (Gag g : lures) {
        if (g.target == -1) {
            // group lure
            for (size_t i = 0; i < c.getSize(); ++i) {
                Cog& cog = c.getCog(i);
                if (c.getCog(i).getHP() != 0) {
                    if (cog.getTrap()) {
                        lured[i] = -1 * cog.getTrap();
                    } else if (!cog.getLured()) {
                        if (g.prestiged) {
                            lured[i] = 2;
                        } else if (!lured[i]) {
                            lured[i] = 1;
                        }
                    }
                }
            }
        } else if (c.getCog(g.target).getHP() != 0) {
            Cog& cog = c.getCog(g.target);
            if (cog.getTrap()) {
                lured[g.target] = -1 * cog.getTrap();
            } else if (!cog.getLured()) {
                if (g.prestiged) {
                    lured[g.target] = 2;
                } else if (!lured[g.target]) {
                    lured[g.target] = 1;
                }
            }
        }
    }
    // apply lure
    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        if (lured[i]) {
            if (lured[i] > 0) {
                // lure
                cog.setLured(lured[i]);
            } else {
                // lure into a trap - take damage, remove trap, don't lure
                cog.hit(-1 * lured[i]);
                cog.untrap();
            }
        }
    }
    cout << LURED << "Lure" << rang::style::reset << "\t";
    c.print(lured);
}

void Battle::sound_turn(vector<Gag> sounds) {
    // find max
    int maxlvl = -1;
    for (size_t i = 0; i < c.getSize(); ++i) {
        if (c.getCog(i).getLevel() > maxlvl) {
            maxlvl = c.getCog(i).getLevel();
        }
    }
    // get damages
    int damage = 0;
    for (Gag g : sounds) {
        // add raw damage
        damage += g.damage;
        // apply prestige bonus
        if (g.prestiged) {
            damage += ceil(maxlvl / 2);
        }
    }
    // apply multiple gag bonus
    if (sounds.size() > 1) {
        damage += ceil(damage * 0.2);
    }
    // damage and print effect

    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        // unlure
        if (cog.getLured()) {
            cog.unlure();
        }
        cog.hit(damage);
    }
    vector<int> affected(c.getSize(), 1);
    cout << "Sound\t";
    c.print(affected);
}

void Battle::squirt_turn(vector<Gag> squirts) {
    vector<int> damages(c.getSize(), 0);
    vector<bool> multi(c.getSize(), false);
    int targ = -1;
    bool sos = false;
    for (Gag g : squirts) {
        if (g.target == -1) {
            // sos gag - hits all
            for (size_t i = 0; i < damages.size(); ++i) {
                if (c.getCog(i).getHP() != 0) {
                    if (damages[i]) {
                        multi[i] = true;
                    }
                    damages[i] += g.damage;
                    c.getCog(i).soak();
                }
            }
            sos = true;
        } else if (c.getCog(g.target).getHP() != 0) {
            // single target gag
            // obtain first hit target for sos combo
            if (targ == -1) {
                targ = g.target;
            }
            if (damages[g.target]) {
                multi[g.target] = true;
            }
            // accumulate raw damage
            damages[g.target] += g.damage;
            // handle soaking
            if (g.prestiged) {
                if (g.target - 1 >= 0 && c.getCog(g.target - 1).getHP() != 0) {
                    c.getCog(g.target - 1).soak();
                }
                if (g.target + 1 < (int)c.getSize() && c.getCog(g.target + 1).getHP() != 0) {
                    c.getCog(g.target + 1).soak();
                }
            }
            c.getCog(g.target).soak();
        }
    }
    // damage and print effect
    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        if (damages[i]) {
            cog.hit(damages[i]);
            if (cog.getLured() == 2) {
                // pres lure knockback
                cog.hit(ceil(damages[i] * 0.65));
                cog.unlure();
            } else if (cog.getLured() == 1) {
                // lure knockback
                cog.hit(ceil(damages[i] * 0.5));
                cog.unlure();
            }
            if (sos && targ != -1) {
                cog.hit(ceil(damages[targ] * 0.2));
            } else if (multi[i]) {
                // combo bonus
                cog.hit(ceil(damages[i] * 0.2));
            }
        }
    }
    cout << SOAKED << "Squirt" << rang::style::reset << "\t";
    c.print(damages);
}

void Battle::zap_turn(vector<Gag> zaps) {
    vector<int> damages(c.getSize(), 0);
    // keep track of cogs that have been jumped in the same turn
    vector<bool> jumped(c.getSize(), false);
    // obtain soaked cogs valid for zapping
    vector<bool> soaked;
    for (size_t i = 0; i < c.getSize(); ++i) {
        soaked.push_back(c.getCog(i).getSoaked());
    }
    for (Gag g : zaps) {
        if (g.target == -1) {
            for (size_t i = 0; i < damages.size(); ++i) {
                damages[i] += g.damage * 3;
            }
            // TODO: double check how sos zap + regular zap work
            continue;
        }
        // examine each zap's effect on all cogs (avoid recalculating on the same cog)
        vector<bool> examined;
        examined.assign(c.getSize(), false);
        size_t jump_count = 0;
        int examine_count = 0;
        int targ = g.target;
        char dir = -1;      // -1 left, 1 right
        int lasttarg = -1;  // keeps track of last cog hit (zap cannot jump more than two spaces)
        float dropoff = g.prestiged ? 0.5 : 0.75;
        while (jump_count < 3 && examine_count < (int)c.getSize() && (lasttarg == -1 || abs(targ - lasttarg) <= 2)) {
            // keep checking until jump count limit reached, all cogs examined, or zap fails to jump
            if (jump_count == 0 && (!soaked[targ] || c.getCog(targ).getHP() == 0)) {  // starting on a dry/dead cog
                if (c.getCog(targ).getHP() != 0) {
                    damages[targ] += g.damage;
                }
                break;
            }
            if ((jump_count == 0 || !jumped[targ]) && soaked[targ] && c.getCog(targ).getHP() != 0) {  // zappable cog
                damages[targ] += ceil((3 - jump_count * dropoff) * g.damage);
                jumped[targ] = true;
                examined[targ] = true;
                lasttarg = targ;
                ++jump_count;
                ++examine_count;
            } else if ((c.getCog(targ).getHP() == 0 || (targ != g.target && (jumped[targ] || !soaked[targ]))) && !examined[targ]) {  // exhausted cog
                ++examine_count;
                examined[targ] = true;
            }
            // change direction if necessary
            if (targ == 0) {
                dir = 1;
            } else if (targ == (int)(c.getSize() - 1)) {
                dir = -1;
            }
            targ += dir;
        }
        // undo "jumped" for the targeted cog
        jumped[g.target] = false;
    }
    // damage and print effect

    for (size_t i = 0; i < c.getSize(); ++i) {
        // cout << damages[i] << endl;
        Cog& cog = c.getCog(i);
        cog.hit(damages[i]);
        if (damages[i]) {
            if (cog.getLured()) {
                cog.unlure();
            }
        }
    }
    cout << ZAPPED << "Zap" << rang::style::reset << "\t";
    c.print(damages);
}

void Battle::throw_turn(vector<Gag> throws) {
    vector<int> damages(c.getSize(), 0);
    vector<bool> multi(c.getSize(), false);
    int targ = -1;
    bool sos = false;
    for (Gag g : throws) {
        if (g.target == -1) {
            // sos gag - hits all
            for (size_t i = 0; i < damages.size(); ++i) {
                if (c.getCog(i).getHP() != 0) {
                    if (damages[i]) {
                        multi[i] = true;
                    }
                    damages[i] += g.damage;
                }
                // damages[i] += g.prestiged ? ceil(g.damage * 1.15) : g.damage;
            }
            sos = true;
        } else if (c.getCog(g.target).getHP() != 0) {
            // single target gag
            // obtain first hit target for sos combo
            if (targ == -1) {
                targ = g.target;
            }
            if (damages[g.target]) {
                multi[g.target] = true;
            }
            // accumulate raw damage
            damages[g.target] += g.prestiged ? ceil(g.damage * 1.15) : g.damage;
        }
    }
    // damage and print effect
    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        if (damages[i]) {
            cog.hit(damages[i]);
            if (cog.getLured() == 2) {
                // pres lure knockback
                cog.hit(ceil(damages[i] * 0.65));
                cog.unlure();
            } else if (cog.getLured() == 1) {
                // lure knockback
                cog.hit(ceil(damages[i] * 0.5));
                cog.unlure();
            }
            if (sos && targ != -1) {
                cog.hit(ceil(damages[targ] * 0.2));
            } else if (multi[i]) {
                // combo bonus
                cog.hit(ceil(damages[i] * 0.2));
            }
        }
    }
    cout << THROWN << "Throw" << rang::style::reset << "\t";
    c.print(damages);
}

void Battle::drop_turn(vector<Gag> drops) {
    vector<int> damages(c.getSize(), 0);
    vector<int> multi(c.getSize(), 0);
    int targ = -1;
    bool sos = false;
    for (Gag g : drops) {
        if (g.target == -1) {
            // sos gag - hits all
            for (size_t i = 0; i < damages.size(); ++i) {
                if (c.getCog(i).getHP() != 0) {
                    damages[i] += g.damage;
                    multi[i] += 10;
                }
            }
            sos = true;
        } else if (c.getCog(g.target).getHP() != 0) {
            // single target gag
            // obtain first hit target for sos combo
            if (targ == -1) {
                targ = g.target;
            }
            // accumulate raw damage
            damages[g.target] += g.damage;
            if (g.prestiged) {
                multi[g.target] += 15;
            } else {
                multi[g.target] += 10;
            }
        }
    }
    // damage and print effect
    for (size_t i = 0; i < c.getSize(); ++i) {
        Cog& cog = c.getCog(i);
        if (damages[i] && !cog.getLured()) {
            cog.hit(damages[i]);
            if (sos && targ != -1) {
                // if sos + another drop, all cogs get hit with combo damage from first cog
                // if just sos, no bonus damage
                cog.hit(ceil(damages[targ] * (multi[targ] + 10) / 100.0));
            } else if (multi[i] > 1) {
                // combo bonus
                cog.hit(ceil(damages[i] * (multi[i] + 10) / 100.0));
            }
        }
    }
    cout << DROPPED << "Drop" << rang::style::reset << "\t";
    c.print(damages);
}

void Battle::main(bool line_input) {
    string strat;
    while (c.getSize() != 0 && strat != "END") {
        // print cogs
        cout << endl << "\t" << c << endl << endl;
        do {
            // get player strategy
            try {
                Battle::Strategy strategy;
                if (line_input) {  // one liner
                    cout << PROMPT << "Your strategy: " << rang::style::reset;
                    getline(cin, strat);
                    if (strat == "END") {
                        cout << "Force stop" << endl;
                        break;
                    }
                    strategy = parse_oneliner(strat);
                } else {  // individual toon directing
                    strategy = parse_gags();
                }
                turn(strategy);
                break;
            } catch (const invalid_argument& e) {
                cerr << e.what() << endl;
            }
        } while (true);
    }
    if (c.getSize() == 0) {
        cout << "You did it!" << endl;
    }
}
