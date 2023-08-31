#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <unordered_map>
#include <chrono>
#pragma comment(lib, "ws2_32.lib")

const int PORT = 50505;
const int BUFFER_SIZE = 2048;



int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Command: SendData <destination_address>" << std::endl;
		return 1;
	}

	const char* destinationAddress = argv[1];

	// Initialize Winsock
	WSADATA wsData;
	if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
		std::cerr << "Failed to initialize Winsock." << std::endl;
		return 1;
	}

	// Create socket
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		std::cerr << "Failed to create socket." << std::endl;
		WSACleanup();
		return 1;
	}

	// Prepare sockaddr_in structure
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr(destinationAddress);

	if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
		std::cerr << "Invalid destination address." << std::endl;
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}
	// Connect to the server
	if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Failed to connect to server." << std::endl;
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	std::string command;
	std::string input;
	char ackBuffer[40];
	int bytesRead = 0;

	while (true) {
		std::cout << "Enter command: ";
		std::getline(std::cin, command);

		if (command == "Exit") {
			break;
		}
		else if (command.find("SendText") == 0) {
			std::cout << "command: " << command << std::endl;
			send(clientSocket, command.c_str(), command.size(), 0);

			std::string text = command.substr(9);

			////Send text size
			long long textSize = text.size();
			std::string textSizeStr = std::to_string(textSize);
			send(clientSocket, textSizeStr.c_str(), textSizeStr.size(), 0);

			std::cout << "Text size: " << textSize << std::endl;
			//send text
			int offset = 0;
			while (offset < text.size()) {
				int chunkSize = (((BUFFER_SIZE) < (static_cast<int>(text.size() - offset))) ? (BUFFER_SIZE) : (static_cast<int>(text.size() - offset)));
				send(clientSocket, text.c_str() + offset, chunkSize, 0);
				offset += chunkSize;
				std::cout << "Text offset: " << offset << std::endl;
			}
			bytesRead = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);
			if (bytesRead > 0) {
				ackBuffer[bytesRead] = '\0';
				std::cout << "Receiver acknowledgment: " << ackBuffer << std::endl;
			}
		}
		else if (command.find("SendFile") == 0) {
			const int WINDOW_SIZE = 4; 
			const int TIMEOUT = 1000; 
			std::unordered_map<int, std::chrono::steady_clock::time_point> packetSendTimes;  // To track packet send times

			int base = 0;              // Base of the sending window
			int nextSeqNum = 0;        // Next sequence number
			std::unordered_map<int, bool> sentPackets;

			//send file
			std::string filePath, bufferSizeStr = "1024";
			int bufferSize = 0;
			std::istringstream iss(command);
			iss >> command >> filePath >> bufferSizeStr;
			if (filePath == "") {
				std::cerr << "File path is invalid!" << std::endl;
				continue;
			}

			std::ifstream file(filePath, std::ios::binary);
			if (!file.is_open()) {

				std::cerr << "Failed to open file" << std::endl;
				continue;
			}
			send(clientSocket, command.c_str(), command.size(), 0);
			

			if (bufferSizeStr != "")
				bufferSize = std::stoi(bufferSizeStr);

			// get the file size
			file.seekg(0, std::ios::end);
			long long fileSize = file.tellg();
			file.seekg(0, std::ios::beg);
			std::cout << "File size: " << fileSize << std::endl;
			// send the file size 
			std::string fileSizeStr = std::to_string(fileSize);
			std::string combinedData = bufferSizeStr + "|" + fileSizeStr;
			send(clientSocket, combinedData.c_str(), combinedData.size(), 0);

			char* buffer = new char[bufferSize];
			int sequenceNumber = 0;

			while (!file.eof()) {
				if (nextSeqNum < base + WINDOW_SIZE) {
					file.read(buffer, bufferSize);
					int bytesRead = static_cast<int>(file.gcount());

					if (bytesRead > 0) {
						unsigned int checksum = 0;
						for (int i = 0; i < bytesRead; ++i) {
							checksum += static_cast<unsigned int>(buffer[i]);
						}
						std::cout << "byte read send:" << bytesRead << std::endl;
						// Send data, checksum, seq
						send(clientSocket, buffer, bytesRead, 0);
						send(clientSocket, reinterpret_cast<char*>(&checksum), sizeof(checksum), 0);
						std::cout << "check sum send:" << checksum << std::endl;
						send(clientSocket, reinterpret_cast<char*>(&nextSeqNum), sizeof(nextSeqNum), 0);
						std::cout << "sequence numb send:" << nextSeqNum << std::endl;
						// Start timer for the packet
						packetSendTimes[nextSeqNum] = std::chrono::steady_clock::now();
						
						// Mark the packet as sent in the sentPackets map
						sentPackets[nextSeqNum] = true;
						nextSeqNum++;
						//Receive ack/nack
						char ackBuffer[4]; 
						int bytesReceived = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);

						bool hasTimedOut = false;
						
						std::chrono::milliseconds timeoutDuration(TIMEOUT);

						if (packetSendTimes.find(base) != packetSendTimes.end()) {
							auto currentTime = std::chrono::steady_clock::now();
							auto sendTime = packetSendTimes[base];

							hasTimedOut = (currentTime - sendTime) >= timeoutDuration;
						}
						else
							hasTimedOut = false;
						 
						if (bytesReceived > 0) {
							ackBuffer[bytesReceived] = '\0';

							if (strcmp(ackBuffer, "ACK") == 0) {
								//slide the window
								base++;
								// Remove the acknowledged packet from the sentPackets map
								sentPackets.erase(base - 1);
							}
							else if (strcmp(ackBuffer, "NACK") == 0 || hasTimedOut) {
								// Resend the packet 
								long long offset = static_cast<long long>(base) * static_cast<long long>(bufferSize);
								file.seekg(offset);
							
								file.read(buffer, bufferSize);
								int bytesRead = file.gcount();

								if (bytesRead > 0) {
									unsigned int checksum = 0;
									for (int i = 0; i < bytesRead; ++i) {
										checksum += static_cast<unsigned int>(buffer[i]);
									}
									// Send the packet data, checksum, and sequence number
									send(clientSocket, buffer, bytesRead, 0);
									send(clientSocket, reinterpret_cast<char*>(&checksum), sizeof(checksum), 0);
									send(clientSocket, reinterpret_cast<char*>(&base), sizeof(base), 0);

									std::cout << "Resent packet with sequence number: " << base << std::endl;
								}
								//restart the timer
								packetSendTimes[base] = std::chrono::steady_clock::now();

							}
						}
						

					}


					
				}
			}
			// ack message
			bytesRead = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);
			if (bytesRead > 0) {
				ackBuffer[bytesRead] = '\0';
				std::cout << "Receiver acknowledgment: " << ackBuffer << std::endl;
			}

			delete[] buffer;
			file.close();
		}
		else {
					std::cerr << "Invalid command. Please use SendText or SendFile. Enter Exit to exit" << std::endl;

		}
	

		// Cleanup
		closesocket(clientSocket);
		WSACleanup();

		return 0;
		
	}
}
