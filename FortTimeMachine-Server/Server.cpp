#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define BUFFER_SIZE 512
#define PORT "8080"

// Make int to store number of players
int PlayerCount = 0;

// Define player position and rotation variables
std::vector<std::string> PlayerLocations;

DWORD WINAPI ProcessClient(LPVOID lpParameter)
{
    SOCKET AcceptSocket = (SOCKET)lpParameter;

    // Send and receive data.
    int bytesSent;
    int bytesRecv = SOCKET_ERROR;
    int recvbuflen = BUFFER_SIZE;
    char sendbuf[BUFFER_SIZE] = "";
    char recvbuf[BUFFER_SIZE] = "";

    while (1)
    {
        bytesRecv = recv(AcceptSocket, recvbuf, BUFFER_SIZE, 0);
        printf("Client said: %s\n", recvbuf);

        std::string recvstr(recvbuf);

        // Tell client what player number they are
        if (recvstr == "Get Player Number") {
            std::string PlayerNum = "Player" + std::to_string(PlayerCount);

            // Respond with player number
            memcpy(sendbuf, PlayerNum.c_str(), PlayerNum.size());

            bytesSent = send(AcceptSocket, sendbuf, PlayerNum.size(), 0);
            if (bytesSent == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(AcceptSocket);
                WSACleanup();
                return 0;
            }

            PlayerCount++;
        }

        if (PlayerLocations.size() > 1) {
            std::string PlayerNumberString = recvstr;
            PlayerNumberString.erase(0, 6);
            int PlayerNumber = std::stoi(PlayerNumberString);

            // Record players location
            PlayerLocations[PlayerNumber] = recvstr;

            for (int i = 0; i < PlayerCount; i++) {
                if (i != PlayerNumber) {
                    // Convert players location string to char
                    memcpy(sendbuf, PlayerLocations[i].c_str(), PlayerLocations[i].size());

                    // Send the buffer to the client
                    bytesSent = send(AcceptSocket, sendbuf, PlayerLocations[i].size(), 0);
                    if (bytesSent == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        closesocket(AcceptSocket);
                        WSACleanup();
                        return 0;
                    }
                }
            }
        }
        else {
            if (PlayerLocations.size() != PlayerCount) {
                for (int i = 0; i < PlayerCount; i++) {
                    if (recvstr.find("Player" + i) != std::string::npos) {
                        PlayerLocations.push_back(recvstr);
                    }
                }
            }

            // Make error message string
            std::string errormsg = "Error";

            // Respond with error
            memcpy(sendbuf, errormsg.c_str(), errormsg.size());

            bytesSent = send(AcceptSocket, sendbuf, errormsg.size(), 0);
            if (bytesSent == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(AcceptSocket);
                WSACleanup();
                return 0;
            }
        }
    }
}

int __cdecl main(void)
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    while (1)
    {
        ClientSocket = SOCKET_ERROR;

        while (ClientSocket == SOCKET_ERROR)
        {
            ClientSocket = accept(ListenSocket, NULL, NULL);
        }

        printf("Client Connected.\n");

        DWORD dwThreadId;
        CreateThread(NULL, 0, ProcessClient, (LPVOID)ClientSocket, 0, &dwThreadId);
    }

    return 0;
}