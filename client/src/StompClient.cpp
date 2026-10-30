#include <stdlib.h>
#include "../include/ConnectionHandler.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <atomic>
#include "../include/StompProtocol.h"

std::atomic<bool> isConnected(false);

void socketListener (ConnectionHandler& handler,StompProtocol& protocol){
	while(isConnected){
		std::string answer;
		if (!handler.getFrameAscii(answer, '\0')){
            std::cout << "Disconnected from server" << std::endl;
            isConnected = false;
            break;
		}
		bool shouldDisconnect=protocol.processServerMessage(answer);
		if (shouldDisconnect)
			isConnected=false;
	}
}

int main(int argc, char *argv[]) {
	while(1){
		const short bufsize = 1024;
        char buf[bufsize];
        if (!std::cin.getline(buf, bufsize)) // reading from kyeboard 
			break;
        std::string line(buf);
		std::stringstream ss(line);
		std::string word;
		std::vector<std::string> words;
		while (ss >> word) { // '>>' operator reads characters until it hits whitespace
			//Add the extracted word to the end of the vector
			words.push_back(word);
		}
		//-------- login case --------
		if (words.size()>0 && words[0]=="login"){
			if (isConnected){
				std::cerr << "The client is already logged in, log out before trying again" << std::endl;
				continue; // waiting for another login
			}
			if (words.size() < 4) {
				std::cerr << "Error: login command needs arguments (host:port user pass)" << std::endl;
				continue; // waiting for another login
			}
			std::string hostPort = words[1];
			std::string username = words[2];
			std::string password = words[3];
			size_t position = hostPort.find(':'); //find the first ":"
			if (position == std::string::npos) {// didn't found ":"
                std::cerr << "Error: host:port format is incorrect." << std::endl;
                continue;
            }
			std::string host = hostPort.substr(0, position);
			std::string portStr = hostPort.substr(position + 1);
			short port = (short)stoi(portStr);
			ConnectionHandler connectionHandler(host, port);// create in every login- socket connection 
			if (!connectionHandler.connect()) {// not able to connect 
				std::cerr << "Could not connect to server"<< std::endl;
				continue; //destory CH
			}	
			std::string frame = "CONNECT\n";
			frame += "accept-version:1.2\n";
			frame += "host:stomp.cs.bgu.ac.il\n";
			frame += "login:" + username + "\n";
			frame += "passcode:" + password + "\n";
			frame += "\n"; 
			if(!connectionHandler.sendFrameAscii(frame, '\0')){
				std::cerr << "Could not connect to server" << std::endl;
				continue;
			}
			std::string connectionResponse;
			if (!connectionHandler.getFrameAscii(connectionResponse, '\0')){
				std::cerr << "Could not connect to server"<< std::endl;
				continue;
			}
			if (connectionResponse.find("CONNECTED")!= std::string::npos){
				//------------------------connection successful!!------------------------
				std::cout <<"Login successful" << std::endl;
				isConnected=true;
				StompProtocol protocol;// we create it per login
				std::thread th(socketListener, std::ref(connectionHandler), std::ref(protocol));// in the constructor of thread we creat copies, std::ref is reference 
				while (isConnected){
					if (!std::cin.getline(buf, bufsize)){
						isConnected = false;
						break;
					}
					std::string actionLine(buf);
					std::string allFrames = protocol.processInput(actionLine, username);
					if (!allFrames.empty()) {
						size_t start = 0;
						size_t end = allFrames.find('\0', start); //find all the first "index-end"
						while (end != std::string::npos) {
							std::string singleFrame = allFrames.substr(start, end - start);
							if (!connectionHandler.sendFrameAscii(singleFrame, '\0')) { // add '\0' to the end
								std::cout << "Failed to send frame to server" << std::endl;
								isConnected = false;
								break;
							}
							start = end + 1; //update starting index for new frame
							end = allFrames.find('\0', start);
						}
					}
				}
				if (th.joinable()){ //very important for scope problems
					th.join(); //wait for listener thread to finish
				}
				isConnected = false;
				
			}else{// we got an error from server
				if (connectionResponse.find("Wrong password")!= std::string::npos){// not found 
					std::cout << "Wrong password" << std::endl;
				}
				else if (connectionResponse.find("User already logged in")!= std::string::npos){// not found 
					std::cout << "User already logged in" << std::endl;
				}
				else{
                    std::cout << "Login failed. Server response: " << connectionResponse << std::endl;
                }
				isConnected = false;
				continue;
			}
		
		}
	}
}
