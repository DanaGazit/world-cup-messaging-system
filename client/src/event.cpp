#include "../include/event.h"
#include "../include/json.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
using json = nlohmann::json;

Event::Event(std::string team_a_name, std::string team_b_name, std::string name, int time,
             std::map<std::string, std::string> game_updates, std::map<std::string, std::string> team_a_updates,
             std::map<std::string, std::string> team_b_updates, std::string discription)
    : team_a_name(team_a_name), team_b_name(team_b_name), name(name),
      time(time), game_updates(game_updates), team_a_updates(team_a_updates),
      team_b_updates(team_b_updates), description(discription)
{
}

Event::~Event()
{
}

const std::string &Event::get_team_a_name() const
{
    return this->team_a_name;
}

const std::string &Event::get_team_b_name() const
{
    return this->team_b_name;
}

const std::string &Event::get_name() const
{
    return this->name;
}

int Event::get_time() const
{
    return this->time;
}

const std::map<std::string, std::string> &Event::get_game_updates() const
{
    return this->game_updates;
}

const std::map<std::string, std::string> &Event::get_team_a_updates() const
{
    return this->team_a_updates;
}

const std::map<std::string, std::string> &Event::get_team_b_updates() const
{
    return this->team_b_updates;
}

const std::string &Event::get_discription() const
{
    return this->description;
}
const std::string &Event::get_name_user() const
{
    return this->user;

}

Event::Event(const std::string &frame_body) : team_a_name(""), team_b_name(""), name(""), time(0), game_updates(), team_a_updates(), team_b_updates(), description("")
{
    std::istringstream ss(frame_body);
    std::string line;
    std::string timeStr;
    std::string currentSection = "";//which section??
    while (std::getline(ss, line)) {
        if (line.find("general game updates:") == 0) {
            currentSection = "general";
            continue;
        }
        if (line.find("team a updates:") == 0) {
            currentSection = "team_a";
            continue;
        }
        if (line.find("team b updates:") == 0) {
            currentSection = "team_b";
            continue;
        }
        if (line.find("description:") == 0) {
            currentSection = "description";
            continue;
        }
        if (line.find("user:") == 0) {
            user = eraseSpace(line.substr(5));
            continue;
        }
        if (line.find("event name:") == 0) {
            name = eraseSpace(line.substr(11));
            continue;
        }
        if (line.find("time:") == 0) {
            timeStr = eraseSpace(line.substr(5));
            continue;
        }
        if (line.find("team a:") == 0) {
            team_a_name = eraseSpace(line.substr(7));
            continue;
        }
        if (line.find("team b:") == 0) {
            team_b_name = eraseSpace(line.substr(7));
            continue;
        }
        if (currentSection == "description") {
            description += line + "\n";
        }
        else if (currentSection == "general" || currentSection == "team_a" || currentSection == "team_b") {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = eraseSpace(line.substr(0, colon));
                std::string value = eraseSpace(line.substr(colon + 1));
                if (currentSection == "general") game_updates[key] = value;
                else if (currentSection == "team_a") team_a_updates[key] = value;
                else if (currentSection == "team_b") team_b_updates[key] = value;
            }
        }
    }
    try {
        time = std::stoi(timeStr);
    } catch (const std::exception& e) {
        std::cerr << "the time is invalid" << std::endl;
        time = 0;
    }
}


names_and_events parseEventsFile(std::string json_path)
{
    std::ifstream f(json_path);
    json data = json::parse(f);

    std::string team_a_name = data["team a"];
    std::string team_b_name = data["team b"];

    // run over all the events and convert them to Event objects
    std::vector<Event> events;
    for (auto &event : data["events"])
    {
        std::string name = event["event name"];
        int time = event["time"];
        std::string description = event["description"];
        std::map<std::string, std::string> game_updates;
        std::map<std::string, std::string> team_a_updates;
        std::map<std::string, std::string> team_b_updates;
        for (auto &update : event["general game updates"].items())
        {
            if (update.value().is_string())
                game_updates[update.key()] = update.value();
            else
                game_updates[update.key()] = update.value().dump();
        }

        for (auto &update : event["team a updates"].items())
        {
            if (update.value().is_string())
                team_a_updates[update.key()] = update.value();
            else
                team_a_updates[update.key()] = update.value().dump();
        }

        for (auto &update : event["team b updates"].items())
        {
            if (update.value().is_string())
                team_b_updates[update.key()] = update.value();
            else
                team_b_updates[update.key()] = update.value().dump();
        }
        
        events.push_back(Event(team_a_name, team_b_name, name, time, game_updates, team_a_updates, team_b_updates, description));
    }
    names_and_events events_and_names{team_a_name, team_b_name, events};

    return events_and_names;
}


std::string Event::eraseSpace(std::string s) {
    const std::string WHITESPACE = " \n\r\t";// t-tab r-start of line
    size_t start = s.find_first_not_of(WHITESPACE);//find the first char
    if (start == std::string::npos) {// if there is nothing in the str:
        return "";
    }
    size_t end = s.find_last_not_of(WHITESPACE);//find the last char
    return s.substr(start, end - start + 1);// just chars
}
