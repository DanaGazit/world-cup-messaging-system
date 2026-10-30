#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include "Game.h"
#include <mutex>

class StompProtocol {
private:
    std::map<std::string, Game> gamesManager;    //game_name-> statistics and events 
    std::map<std::string, int> gameToSubId;      
    std::map<int, std::string> receiptToCommand; // recipt id -> command 

    
    int subIdCounter = 0;
    int receiptIdCounter = 0;
    std::mutex _lock_A;
    std::mutex _lock_B;

public:
    StompProtocol();
    std::string processInput(std::string input, std::string username);
    bool processServerMessage(std::string message);

private:
    bool handleReceipt(std::string msg);
    void handleMessage(std::string message);
    void handleSummaryCommand(std::stringstream& ss);
    std::string handleReport(std::stringstream& ss, std::string username);
};