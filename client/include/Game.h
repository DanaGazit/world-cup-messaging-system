#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "event.h"

class Game {
private:
std::string teamAname;
    std::string teamBname;
    std::map<std::string, std::string> general_stats;
    std::map<std::string, std::string> teamAstats;
    std::map<std::string, std::string> teamBstats;
    std::multimap<int, Event> events; // Time -> Event. Kept sorted. Used multimap to allow multiple events at the same timestamp.
public:
    Game() : teamAname(""), teamBname(""), general_stats(), teamAstats(), teamBstats(), events() { //empty builder
    }

    Game(std::string name) : teamAname(""), teamBname(""), general_stats(), teamAstats(), teamBstats(), events() {
        size_t underscore = name.find('_'); //seperates A and B teams
        if (underscore != std::string::npos) {
            teamAname = name.substr(0, underscore);
            teamBname = name.substr(underscore + 1);
        } else {
            std::cerr << "Warning: Game name '" << name << "' is not in TeamA_TeamB format!" << std::endl;
            teamAname = name;
            teamBname = "Unknown";
        }
    }
    // --- Getters ---
    std::string getTeamAName() const {
        return teamAname; }
    std::string getTeamBName() const {
        return teamBname; }
    const  std::multimap<int, Event>& getEvents() const {
        return events; }
    const std::map<std::string, std::string>& getGeneralStats() const {
        return general_stats; }
    const std::map<std::string, std::string>& getTeamAStats() const {
        return teamAstats; }
    const std::map<std::string, std::string>& getTeamBStats() const {
        return teamBstats; }

    void setTeamAName(std::string name) { teamAname = name; }
    void setTeamBName(std::string name) { teamBname = name; }

    void addEvent(const Event& e){
        int time=e.get_time();
        events.insert({time, e});
        updateStats(e);
    }
    void updateStats(const Event& e) {
    // general
    for (auto& kv : e.get_game_updates()) {
        general_stats[kv.first] = kv.second;
    }
    //Team A
    for (auto& kv : e.get_team_a_updates()) {
        teamAstats[kv.first] = kv.second;
    }
    //Team B
    for (auto& kv : e.get_team_b_updates()) {
        teamBstats[kv.first] = kv.second;
    }
}
};