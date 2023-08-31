#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <sstream>
#include <unordered_map>
#include <unordered_map>
#include <chrono>
#pragma comment(lib, "ws2_32.lib")

const int PORT = 50505;
const int BUFFER_SIZE = 2048;
const int TIMEOUT = 1000;


int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }


    if (argc < 3 || std::string(argv[1]) != "--out") {
        std::cerr << "Command: ReceiveData --out <location_store_file>" << std::endl;
        WSACleanup();
        return 1;
    }

    const char* locationStoreFile = argv[2];

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on socket." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client connection
    sockaddr_in clientAddr{};
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to accept client connection." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::string outDirectory = argv[2];


    char* buffer = new char[2048];
    int totalBytesReceived = 0;
    std::ofstream outFile;
    bool receivingFile = false;
    long long receivedFileSize = 0;
    long long receivedTextSize = 0;
    std::string filePath = "";
    char bufferTextSize[20];
    int expectedSequenceNumber = 0;
    int bufferReceivedFileSize = 0;
    int bufferSize = 2048;
    std::unordered_map<int, bool> receivedPackets;
    while (true) {
        int bytesRead = recv(clientSocket, buffer, bufferSize, 0);
        
        if (bytesRead <= 0) {
            break;
            std::cout << "Done: "<< std::endl;
        }
        buffer[bytesRead] = '\0';
        
        if (!receivingFile) {
            std::string command(buffer);
           
            if (command.find("SendText") == 0) {
                std::cout << "Send Text here: " << std::endl;
            
                char bufferTextSize[20];
                int bytesRead = recv(clientSocket, bufferTextSize, sizeof(bufferTextSize), 0);
                std::string receivedText = "";
                if (bytesRead > 0) {
                  
                    bufferTextSize[bytesRead] = '\0';
                  
                    long long receivedTextSize = std::stoll(bufferTextSize); 
                    std::cout << "received text size" << receivedTextSize << std::endl;

                    char* bufferText = new char[receivedTextSize + 1];  
                    long long bufferTxtSize = 2048;
                    long long totalReceived = 0;

                    while (totalReceived < receivedTextSize) {
                        int chunkBytesRead = recv(clientSocket, bufferText, bufferTxtSize, 0);
                        if (chunkBytesRead <= 0) {
                            break;
                        }
                        totalReceived += chunkBytesRead;
                        bufferText[chunkBytesRead] = '\0';
                        receivedText += bufferText;
                        std::cout << "Received text: " << receivedText << std::endl;
                        std::cout << "total text: " << totalReceived << std::endl;
                    }

                    std::cout << "Received text: " << receivedText << std::endl;
                    delete[] bufferText;  
                }

                

                std::string ackMessage = "Text content received successfully";
                send(clientSocket, ackMessage.c_str(), ackMessage.size(), 0);
            }
            else if (command.find("SendFile") == 0) {
                // Reset for next transmission
                totalBytesReceived = 0;
                receivingFile = true;
                //receive bufferSize
                char bufferSend[100];
                bytesRead = recv(clientSocket, bufferSend, sizeof(bufferSend) - 1, 0);
                bufferSend[bytesRead] = '\0';

                std::string combinedDataReceived(bufferSend);

             
                size_t separatorPos = combinedDataReceived.find('|');
                if (separatorPos != std::string::npos) {
                    delete[] buffer;
                    std::string bufferSizeStr = combinedDataReceived.substr(0, separatorPos);
                   bufferReceivedFileSize = std::stoll(bufferSizeStr);
                   buffer = new char[bufferReceivedFileSize];

                   bufferSize = bufferReceivedFileSize;
                   std::cout << "buffer file size: " << bufferReceivedFileSize << std::endl;
                    std::string fileSizeStr = combinedDataReceived.substr(separatorPos + 1);

                    filePath = outDirectory + "\\received_file" + fileSizeStr;
                    receivedFileSize = std::stoll(fileSizeStr);
                    std::cout << "received file size: " << receivedFileSize << std::endl;
                }

               
                outFile.open(filePath, std::ios::binary);

                std::cout << "Receiving file: " << filePath << std::endl;
                receivingFile = true;
            }
        }
        else {
           
            unsigned int calculatedChecksum = 0;
            std::cout << "byte read receiver:" << buffer << std::endl;
            for (int i = 0; i < bytesRead; ++i) {
                calculatedChecksum += static_cast<unsigned int>(buffer[i]);
            }
            std::cout << "calculated Checksum receive:" << calculatedChecksum << std::endl;
            unsigned int receivedChecksum = 0;
            recv(clientSocket, reinterpret_cast<char*>(&receivedChecksum), sizeof(receivedChecksum), 0);
            std::cout << "checksum receive:" << receivedChecksum << std::endl;
            int receivedSequenceNumber = 0;
            recv(clientSocket, reinterpret_cast<char*>(&receivedSequenceNumber), sizeof(receivedSequenceNumber), 0);
            std::cout << "sequence numb receive:" << receivedSequenceNumber << std::endl;
            

            if (calculatedChecksum == receivedChecksum && receivedSequenceNumber == expectedSequenceNumber) {
             
                outFile.write(buffer, bytesRead);
                std::cout << "Packet integrity verified." << std::endl;
                receivedPackets[expectedSequenceNumber] = true;
                send(clientSocket, "ACK", sizeof("ACK"), 0);
               
                while (receivedPackets.find(expectedSequenceNumber) != receivedPackets.end()) {
                    receivedPackets.erase(expectedSequenceNumber);
                    expectedSequenceNumber++;
                }
                totalBytesReceived += bytesRead;
                float percentReceived = static_cast<float>(totalBytesReceived) / receivedFileSize * 100.0f;
                std::cout << "Received: " << totalBytesReceived << " bytes (" << percentReceived << "%)" << std::endl;
            }
            else {
                std::cout << "Packet integrity compromised. Requesting retransmission..." << std::endl;
             
                // NACK
                send(clientSocket, "NACK", sizeof("NACK"), 0);
                send(clientSocket, reinterpret_cast<char*>(&expectedSequenceNumber), sizeof(expectedSequenceNumber), 0);
              

            }
            

           //// Check receiving file is complete
           if (totalBytesReceived >= receivedFileSize) {
               receivingFile = false;
               outFile.close();

               std::cout << "File received and saved in: " << filePath << std::endl;
              std::string ackMessage = "File content received successfully";
              send(clientSocket, ackMessage.c_str(), ackMessage.size(), 0);
           }

        }
    }


    if (outFile.is_open()) {
        outFile.close();
        std::cout << "File received and saved in: " << filePath << std::endl;
    }
    delete[] buffer;
    //clean up
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
