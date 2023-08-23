#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <sstream>
#pragma comment(lib, "ws2_32.lib")

const int PORT = 50505;
const int BUFFER_SIZE = 2048;


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


    char buffer[BUFFER_SIZE];
    int totalBytesReceived = 0;
    std::ofstream outFile;
    bool receivingFile = false;
    long long receivedFileSize = 0;
    long long receivedTextSize = 0;
    std::string filePath = "";
    char bufferTextSize[BUFFER_SIZE];

    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            break;
        }
        buffer[bytesRead] = '\0';

        if (!receivingFile) {
            std::string command(buffer);

            if (command.find("SendText") == 0) {
                //Receive text size
                bytesRead = recv(clientSocket, bufferTextSize, sizeof(bufferTextSize), 0);
                if (bytesRead > 0) {
                    bufferTextSize[bytesRead] = '\0';

                    receivedTextSize = std::stoll(bufferTextSize);
                    std::cout << "Received text size: " << receivedTextSize << std::endl;
                }

                std::string receivedText = "";

                //receive text
                std::cout << "Text size:" << receivedTextSize << std::endl;
                while (receivedText.size() < receivedTextSize) {
                    int chunkBytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                    if (chunkBytesRead <= 0) {
                        break;
                    }
                    buffer[chunkBytesRead] = '\0';
                    receivedText += buffer;
                }
                std::cout << "Received text: " << receivedText << std::endl;

                std::string ackMessage = "Text content received successfully";
                send(clientSocket, ackMessage.c_str(), ackMessage.size(), 0);
            }
            else if (command.find("SendFile") == 0) {
                // Reset for next transmission
                totalBytesReceived = 0;
                receivingFile = true;

                // Receive file size
                bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << "Received file size string: " << buffer << std::endl;
                    receivedFileSize = std::stoll(buffer);
                }

                filePath = outDirectory + "\\received_file" + buffer;
                outFile.open(filePath, std::ios::binary);

                std::cout << "Receiving file: " << filePath << std::endl;
                receivingFile = true;
            }
        }
        else {
            // Receive and write file data
            outFile.write(buffer, bytesRead);
            totalBytesReceived += bytesRead;
            float percentReceived = static_cast<float>(totalBytesReceived) / receivedFileSize * 100.0f;
            std::cout << "Received: " << totalBytesReceived << " bytes (" << percentReceived << "%)" << std::endl;

            // Check receiving file is complete
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

    //clean up
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
