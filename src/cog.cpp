#include "cog.h"

#include "rang.h"
#include "colors.h"

Cog& Cog::operator=(const Cog& other) {
    level = other.level;
    hp = other.hp;
    soaked = other.soaked;
    lured = other.lured;
    executive = other.executive;
    return *this;
}

std::string Cog::getLevelName() {
    if (level == 0) {
        return "";
    } else if (executive) {
        return std::to_string(level) + ".exe";
    }
    return std::to_string(level);
}

void Cog::hit(int damage) { // deal raw damage
    hp -= damage;
    if (hp < 0) {
        hp = 0;
    }
}

std::ostream& operator<<(std::ostream& out, const Cog& cog) {
    if (cog.soaked) {
        out << SOAKED;
    }
    if (cog.lured > 0) {
        out << LURED;
    }
    if (cog.hp == 0) {
        out << DEAD;
    }
    if (cog.level == 0) {
    } else if (cog.executive) {
        out << "Level " << std::to_string(cog.level) << ".exe: " << cog.hp;
    } else {
        out << "Level " << std::to_string(cog.level) << ": " << cog.hp;
    }
    out << rang::style::reset;
    return out;
}
