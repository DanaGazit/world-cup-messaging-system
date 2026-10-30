#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <sstream>
#include <fstream>

StompProtocol::StompProtocol() : gamesManager(), gameToSubId(), receiptToCommand(), _lock_A(),_lock_B() {}

std::string StompProtocol::processInput(std::string input, std::string username){
    std::stringstream ss(input);
    std::string command;
    ss >> command; //first word

    if (command == "join") {
        std::string gameName;
        ss >> gameName; //second word is game name to join
        int subId = subIdCounter++;
        int receiptId = receiptIdCounter++;
        gameToSubId[gameName] = subId;
        std::lock_guard<std::mutex> lock(_lock_A);
        receiptToCommand[receiptId] = "Joined channel " + gameName; //it's ok to join channel twice
        return "SUBSCRIBE\ndestination:/" + gameName + "\nid:" + std::to_string(subId) + 
               "\nreceipt:" + std::to_string(receiptId) + "\n\n\0";
    }

    else if (command== "exit") {
        std::string gameName;
        ss >> gameName;
        if (gameToSubId.count(gameName)) { //if there is one id with that game name
            int subId = gameToSubId[gameName];
            int receiptId = receiptIdCounter++;
            std::lock_guard<std::mutex> lock(_lock_A); 
            receiptToCommand[receiptId] = "Exited channel " + gameName;
            gameToSubId.erase(gameName);
            return "UNSUBSCRIBE\nid:" + std::to_string(subId) + "\nreceipt:" + std::to_string(receiptId) + "\n\n\0";
        }
        else {
            std::cout << "You are not subscribed to " << gameName << std::endl;
            return "";
        }
    }
    else if (command== "logout"){
        int receiptId = receiptIdCounter++;
        std::lock_guard<std::mutex> lock(_lock_A);
        receiptToCommand[receiptId] = "logout";
        return "DISCONNECT\nreceipt:" + std::to_string(receiptId) + "\n\n\0";
    }
    else if (command == "report"){
        return handleReport(ss,username); //sends many frames
        
    }

    else if (command== "summary") {
        handleSummaryCommand(ss); 
        return ""; 
    }

    std::cout << "no such FRAME as: " << command << std::endl;
    return "";
}

bool StompProtocol::processServerMessage(std::string message) { //true- should disconnect, false- still connected
    std::stringstream ss(message);
    std::string firstLine;
    std::getline(ss, firstLine);

    if (firstLine.find("CONNECTED") != std::string::npos) { //not supposed to arrive here
        std::cout << "Login successful" << std::endl;
        return false;
    } 
    else if (firstLine.find("RECEIPT") != std::string::npos) {
        return handleReceipt(message); 

    } 
    else if (firstLine.find("MESSAGE") != std::string::npos) {
        handleMessage(message);

    } 
    else if (firstLine.find("ERROR") != std::string::npos) {
        std::cout << "Error from server: " << message << std::endl;
        return false;
    }
    return false;
}

bool StompProtocol::handleReceipt(std::string msg) {//true- should disconnect, false- still connected
    size_t pos = msg.find("receipt-id:"); //finding index of "receipt-id:"
    if (pos != std::string::npos) { 
        std::string idStr = msg.substr(pos + 11); //length of 'receipt-id:' == 11
        int rID;
        try{
            rID = std::stoi(idStr);
        }catch (const std::exception& e) {//if stoi throws an error
            std::cerr << "error parsing receipt ID" << std::endl;
            return false;
        }
        std::lock_guard<std::mutex> lock(_lock_A); 
        if (receiptToCommand.count(rID)){ //we have atleast 1 receipt-id such as rID
            std::string commandInfo = receiptToCommand[rID];
            std::cout << commandInfo << std::endl;
            receiptToCommand.erase(rID);
            if (commandInfo=="logout") 
                return true; //isConnected = false
        }
    }
    else{//malfored msg, the receipt-id doesn't exist in msg
        std::cerr << "Error: no receipt-id" << std::endl;
    } 
    return false; 
}

void StompProtocol::handleMessage(std::string message) {
    std::stringstream ss(message);
    std::string line;
    std::string gameName = "";
    while (std::getline(ss, line) && line != "") { //lopping all the headers and find destinatio
        if (line.find("destination:") ==0) { //returns if this line is destinatio
            size_t slashPos = line.rfind('/'); // find the last /
            if (slashPos != std::string::npos) {
                gameName = line.substr(slashPos + 1); //until the end 
            }
        }
    }
    if (gameName==""){//didn't find the game name 
        return;
    }
    //---- body----
    std::string body;// we need to sand only the body
    while (std::getline(ss, line)) { 
        body += line + "\n";
    } 
    Event e(body);
    std::lock_guard<std::mutex> lock(_lock_B); 
    auto found = gamesManager.find(gameName);
    if(found ==gamesManager.end()){//gamesManager.end() pointing to the end of the map
        Game newGame(gameName);
        newGame.addEvent(e);
        gamesManager[gameName]=newGame;
    }
    else{
        found->second.addEvent(e);
    }
}

void StompProtocol:: handleSummaryCommand(std::stringstream& ss) {
    std::string gameName, user,fileName;
    ss >> gameName >> user >> fileName;
    
    std::stringstream outputBuffer; 
    { //lock block
        std::lock_guard<std::mutex> lock(_lock_B);
        auto iter = gamesManager.find(gameName); //return iter on map that point to <game name, GAME>
        if (iter == gamesManager.end()) {
            std::cerr << "Game " << gameName << " not found! you shold try agin" << std::endl;
            return;
        }
        Game& game = iter->second; //reference 
        outputBuffer << game.getTeamAName() << " vs " << game.getTeamBName() << "\n\n";
        outputBuffer << "Game stats:\n";
        outputBuffer << "General Stats:\n";
        for(const auto& kv : game.getGeneralStats()) {
                outputBuffer << kv.first << ": " << kv.second << "\n";
        }
        outputBuffer << "\nTeam A Stats (" << game.getTeamAName() << "):\n";
        for (const auto& kv : game.getTeamAStats()){
            outputBuffer << kv.first << ": " << kv.second << "\n";
        }
        outputBuffer << "\nTeam B Stats (" << game.getTeamBName() << "):\n";
        for (const auto& kv : game.getTeamBStats()) {
            outputBuffer << kv.first << ": " << kv.second << "\n";
        }
        outputBuffer << "\nEvents from user " << user << ":\n";
        for (auto& kv : game.getEvents()) {
            const Event& e = kv.second;
            if(e.get_name_user() == user){
                outputBuffer << e.get_time() << " - " << e.get_name() << "\n";
                outputBuffer << e.get_discription() << "\n\n";
            }
        }
    }// end lock block
     
    std::ofstream out(fileName);// new object output file ofstream, if the file exsist the content will deleted
    if (!out.is_open()) { // if there is an issue with the file 
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return;
    }
    out << outputBuffer.rdbuf();
    out.close();
    //success msg
    std::cout << "Summary for game " << gameName
              << " from user " << user
              << " written to " << fileName << std::endl;
}

std::string StompProtocol::handleReport(std::stringstream& ss, std::string username) {
    std::string fileName;
    if (!(ss >> fileName)){
        std::cout << "Error: Missing filename for: report {filename}" << std::endl;//not reporting
        return "";
    }
    names_and_events data = parseEventsFile(fileName);
    std::string gameName = data.team_a_name + "_" + data.team_b_name;
    if (gameToSubId.find(gameName)==gameToSubId.end()){ //checking if client is sub to this game
        std::cout << "Error: You must be subscribed to " << gameName << " to report events." << std::endl;//not reporting
        return "";
    }
  
    std::string allFrames= "";
    for (const Event& e : data.events) {//for every event:
        std::stringstream frame;
    
        frame << "SEND\n";
        frame << "destination:/" << gameName << "\n";
        frame << "file: " << fileName << "\n\n"; // adding header file name
        frame << "user: " << username << "\n"; 
        frame << "team a: " << e.get_team_a_name() << "\n";
        frame << "team b: " << e.get_team_b_name() << "\n";
        frame << "event name: " << e.get_name() << "\n";
        frame << "time: " << e.get_time() << "\n";
        frame << "general game updates:\n";
        for (const auto& it : e.get_game_updates()){//const ref to avoide copying
            frame << it.first << ": " << it.second << "\n"; //it.first = key, it.second' value, end result= key: value
        }
        frame << "team a updates:\n";
        for (const auto& it : e.get_team_a_updates()){
            frame << it.first << ": " << it.second << "\n";
        }
        frame << "team b updates:\n";
        for (const auto& it : e.get_team_b_updates()){
            frame << it.first << ": " << it.second << "\n";
        }
        
        frame << "description:\n" << e.get_discription() << "\n";
        frame << '\0'; //end of event
        allFrames += frame.str();
    }
    return allFrames;
}
