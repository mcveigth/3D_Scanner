//
// Created by mcveigth on 2023-07-15
//
#include <iostream>
#include <string>
#include <ctime>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fstream>
#include <pthread.h>
#include <libgen.h>

#define SIZE 2500

/**
 * structure to hold client information
 */
struct ClientInfo {
    int clientSocket;
    std::string filename;
    std::string savePath;
};

/**
 * @brief Connects to udp multicast server and triggers clients to take pictures
 */
void triggerCapture() {
    int charid = 1;
    int unitid = 1;
    int groupid = 1;

    std::time_t currentTime = std::time(nullptr);
    std::tm *gtdate = std::gmtime(&currentTime);
    std::string now = std::to_string(gtdate->tm_year + 1900) + std::to_string(gtdate->tm_mon + 1)
                      + std::to_string(gtdate->tm_mday) + std::to_string(gtdate->tm_hour)
                      + std::to_string(gtdate->tm_min) + std::to_string(gtdate->tm_sec);

    std::string SDATA = now;

    std::cout << "Sending: " << SDATA << std::endl;

    const char *MCAST_GRP = "224.1.1.1";
    int MCAST_PORT = 5007;

    std::string SCMD(1, static_cast<char>(charid));
    std::string SUNIT(1, static_cast<char>(unitid));
    std::string SGROUP(1, static_cast<char>(groupid));
    std::string SEND = SCMD + SUNIT + SGROUP + SDATA;

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }

    int enableMulticast = 2;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &enableMulticast, sizeof(enableMulticast)) == -1) {
        std::cerr << "Failed to set socket options." << std::endl;
        close(sockfd);
        return;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MCAST_GRP);
    addr.sin_port = htons(MCAST_PORT);

    if (sendto(sockfd, SEND.c_str(), SEND.length(), 0, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        std::cerr << "Failed to send data." << std::endl;
    }

    std::cout << "Sent." << std::endl;

    close(sockfd);
    std::cout << "Socket closed." << std::endl;
}

/**
 * handles each client connection in a separate thread
 * @param arg
 * @return
 */
void *handleClient(void *arg) {
    ClientInfo *clientInfo = static_cast<ClientInfo *>(arg);

    int clientSocket = clientInfo->clientSocket;
    std::string filename = clientInfo->filename;
    std::string savePath = clientInfo->savePath;

    // Extract the filename from the full path
    char *extractedFilename = basename(&filename[0]);
    // Receive the image file content from the client
    const int bufferSize = 1024;
    char buffer[bufferSize];
    std::ofstream outputFile(savePath + "/" + extractedFilename, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Failed to open output file" << std::endl;
        close(clientSocket);
        delete clientInfo;
        return nullptr;
    }

    int bytesRead;
    while ((bytesRead = recv(clientSocket, buffer, bufferSize, 0)) > 0) {
        outputFile.write(buffer, bytesRead);
    }

    if (bytesRead == -1) {
        std::cerr << "Failed to receive image file" << std::endl;
        close(clientSocket);
        outputFile.close();
        delete clientInfo;
        return nullptr;
    }
    std::cout << "Image file received successfully" << savePath << "/" << extractedFilename << std::endl;

    // Close the client socket and the output file
    close(clientSocket);
    outputFile.close();
    delete clientInfo;

    return nullptr;
}

/**
 * @brief tcp server for receiving images from clients
 */
void tcpServer(int PORT, std::string savePath) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        exit(1);
    }

    struct sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(serverSocket);
        exit(1);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1) {
        std::cerr << "Failed to listen" << std::endl;
        close(serverSocket);
        exit(1);
    }
    std::cout << "Listening for incoming connections" << std::endl;


    // Accept a client connection and handle each client in a separate thread
    while (true) {
        // Accept a client connection
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == -1) {
            std::cerr << "Failed to accept client connection" << std::endl;
            continue;
        }

        // Receive the filename from the client
        char filenameBuffer[1024];
        int filenameBytes = recv(clientSocket, filenameBuffer, sizeof(filenameBuffer), 0);
        if (filenameBytes <= 0) {
            std::cerr << "Failed to receive filename" << std::endl;
            close(clientSocket);
            continue;
        }
        std::string filename(filenameBuffer, filenameBytes);

        // Create a ClientInfo struct to hold client socket and filename
        ClientInfo *clientInfo = new ClientInfo;
        clientInfo->clientSocket = clientSocket;
        clientInfo->filename = filename;
        clientInfo->savePath = savePath;

        // Create a new thread to handle the client
        pthread_t thread;
        if (pthread_create(&thread, nullptr, handleClient, clientInfo) != 0) {
            std::cerr << "Failed to create a thread for client" << std::endl;
            close(clientSocket);
            delete clientInfo;
        }
    }

    close(serverSocket);
}

/**
 * @brief handles trigger of the cameras
 */
void userTrigger() {
    std::string input;
    while (true) {
        std::cout << "Start capturing? (y/n)";
        std::getline(std::cin, input);

        if (input == "y") {
            triggerCapture();
            break;
        }
    }
}

int main(int argc, char *argv[]) {
//    userTrigger();
//    triggerCapture();
    tcpServer(8888, "/home/mcveigth/CLionProjects/3Dscanner/3D-Scanner");
    return 0;
}