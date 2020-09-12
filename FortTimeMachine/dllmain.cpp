#include "SDK.hpp"

#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <sstream>

#include <winsock.h>
#pragma comment (lib, "ws2_32.lib")

#include <MinHook.h>
#pragma comment(lib, "minhook.lib")

#include "core.h"
#include "util.h"

#define BUFFER_SIZE 512

SDK::APlayerPawn_Athena_C* pPlayerPawn_Athena_C;
SDK::APlayerPawn_Athena_C* pPlayerPawn_Athena_C2;

PVOID(*ProcessEvent)(SDK::UObject*, SDK::UFunction*, PVOID) = nullptr;

PVOID ProcessEventHook(SDK::UObject* pObject, SDK::UFunction* pFunction, PVOID pParams) {
    if (pObject && pFunction) {
        if (pFunction->GetName().find("ServerAttemptAircraftJump") != std::string::npos) {
            // Use BugItGo, so that the PlayerPawn isn't messed up.
            SDK::FVector actorLocation = Core::pPlayerController->K2_GetActorLocation();
            Core::pPlayerController->CheatManager->BugItGo(actorLocation.X, actorLocation.Y, actorLocation.Z, 0, 0, 0);

            // Summon a new PlayerPawn.
            std::string sClassName = "PlayerPawn_Athena_C";
            Core::pPlayerController->CheatManager->Summon(SDK::FString(std::wstring(sClassName.begin(), sClassName.end()).c_str()));

            // Find our newly summoned PlayerPawn.
            pPlayerPawn_Athena_C = static_cast<SDK::APlayerPawn_Athena_C*>(Util::FindActor(SDK::APlayerPawn_Athena_C::StaticClass()));
            if (!pPlayerPawn_Athena_C)
                printf("Finding PlayerPawn_Athena_C has failed, bailing-out immediately!\n");
            else {
                // Find our SkeletalMesh in UObject cache.
                auto pSkeletalMesh = SDK::UObject::FindObject<SDK::USkeletalMesh>(Core::SKELETAL_MESH);
                if (pSkeletalMesh == nullptr)
                    printf("Finding SkeletalMesh has failed, bailing-out immediately!\n");
                else {
                    pPlayerPawn_Athena_C->Mesh->SetSkeletalMesh(pSkeletalMesh, true);

                    Core::pPlayerController->Possess(pPlayerPawn_Athena_C);
                }
            }
        }

        // HACK: This will probably cause a crash, but it's worth a try.
        if (pFunction->GetName().find("StopHoldProgress") != std::string::npos)
            return NULL;
    }

    return ProcessEvent(pObject, pFunction, pParams);
}

DWORD UpdateThread(LPVOID lpParam) {
    while (1) {
        // Keybind to jump (only run if not skydiving, might need to fix this more though):
        if (GetKeyState(VK_SPACE) & 0x8000 && pPlayerPawn_Athena_C && !pPlayerPawn_Athena_C->IsSkydiving()) {
            if (!pPlayerPawn_Athena_C->IsJumpProvidingForce())
                pPlayerPawn_Athena_C->Jump();
        }

        // Keybind to sprint (only run if not skydiving):
        if (GetKeyState(VK_SHIFT) & 0x8000 && pPlayerPawn_Athena_C && !pPlayerPawn_Athena_C->IsSkydiving())
            pPlayerPawn_Athena_C->CurrentMovementStyle = SDK::EFortMovementStyle::Sprinting;
        else if (pPlayerPawn_Athena_C)
            pPlayerPawn_Athena_C->CurrentMovementStyle = SDK::EFortMovementStyle::Running;

        /*
        // Keybind to equip weapon:
        if (GetKeyState(VK_END) & 0x8000 && pPlayerPawn_Athena_C) {
            auto pFortWeapon = static_cast<SDK::AFortWeapon*>(Util::FindActor(SDK::AFortWeapon::StaticClass()));
            if (!pFortWeapon)
                printf("Finding FortWeapon has failed, bailing-out immediately!\n");
            else {
                pFortWeapon->ClientGivenTo(pPlayerPawn_Athena_C);

                pPlayerPawn_Athena_C->ClientInternalEquipWeapon(pFortWeapon);
            }
        }
        */

        /*
        if (GetKeyState(VK_INSERT) & 0x8000) {
            std::string sClassName = "PlayerPawn_Athena_C";
            Core::pPlayerController->CheatManager->Summon(SDK::FString(std::wstring(sClassName.begin(), sClassName.end()).c_str()));
        }
        */

        if (GetKeyState(VK_HOME) & 0x8000) {
            pPlayerPawn_Athena_C2 = static_cast<SDK::APlayerPawn_Athena_C*>(Util::FindActor(SDK::APlayerPawn_Athena_C::StaticClass(), 1));
            if (!pPlayerPawn_Athena_C2)
                printf("Finding PlayerPawn_Athena_C has failed, bailing-out immediately!\n");
            else {
                // Find our SkeletalMesh in UObject cache.
                auto pSkeletalMesh = SDK::UObject::FindObject<SDK::USkeletalMesh>(Core::SKELETAL_MESH);
                if (pSkeletalMesh == nullptr)
                    printf("Finding SkeletalMesh has failed, bailing-out immediately!\n");
                else {
                    pPlayerPawn_Athena_C2->Mesh->SetSkeletalMesh(pSkeletalMesh, true);
                }
            }
        }

        // Update thread only runs at 60hz, so we don't rape CPUs.
        Sleep(1000 / 60);
    }

    return NULL;
}

DWORD ServerThread(LPVOID lpParam) {
    // Disable world bounds checks so it doesnt kill other playerpawns
    Core::pLevel->WorldSettings->bEnableWorldBoundsChecks = false;
    
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    int iResult;
    int recvbuflen = BUFFER_SIZE;

    char sendbuf[BUFFER_SIZE] = "";
    char recvbuf[BUFFER_SIZE] = "";

    const char* ipaddress = "25.18.6.19";
    int port = 8080;

    struct sockaddr_in serveraddr;
    WSAStartup(MAKEWORD(2, 0), &wsaData);
    iResult = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ConnectSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(ipaddress);
    serveraddr.sin_port = htons((unsigned short)port);

    // Connect to server.
    iResult = connect(ConnectSocket, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
        return 1;
    }

    printf("Initialized server connection\n");

    // Get player number
    std::string getplayermsg = "Get Player Number";
    memcpy(sendbuf, getplayermsg.c_str(), getplayermsg.size());

    send(ConnectSocket, sendbuf, getplayermsg.size(), 0);

    recv(ConnectSocket, recvbuf, recvbuflen, 0);
    std::string PlayerNum(recvbuf);

    std::vector<SDK::APlayerPawn_Athena_C*> pPlayerPawn_Athena_C_Index;

    while (1) {
        // Get player's location and rotation
        SDK::FVector localLocation = pPlayerPawn_Athena_C->K2_GetActorLocation();
        SDK::FRotator localRotation = pPlayerPawn_Athena_C->K2_GetActorRotation();

        // Format location and rotation into string to send it to the server
        std::string localLocationString = PlayerNum + "," + std::to_string(localLocation.X) + "," + std::to_string(localLocation.Y) + "," + std::to_string(localLocation.Z) + "," + std::to_string(localRotation.Pitch) + "," + std::to_string(localRotation.Yaw) + "," + std::to_string(localRotation.Roll);

        // Convert std string to char
        memcpy(sendbuf, localLocationString.c_str(), localLocationString.size());

        // Send buffer
        send(ConnectSocket, sendbuf, localLocationString.size(), 0);

        // Recieve buffer
        recv(ConnectSocket, recvbuf, recvbuflen, 0);

        // Convert char into std string
        std::string recvstr(recvbuf);
        //std::cout << recvstr << std::endl;

        if (recvstr.find("Player") != std::string::npos) {
            std::vector<std::string> vect;
            std::stringstream ss(recvstr);

            while (ss.good()) {
                std::string substr;
                getline(ss, substr, ',');
                vect.push_back(substr);
            }

            SDK::FVector location;
            location.X = std::stof(vect[1]);
            location.Y = std::stof(vect[2]);
            location.Z = std::stof(vect[3]);
            
            SDK::FRotator rotation;
            rotation.Pitch = std::stof(vect[4]);
            rotation.Yaw = std::stof(vect[5]);
            rotation.Roll = std::stof(vect[6]);

            vect[0].erase(0, 6);
            int PlayerNumber = atoi(vect[0].c_str());
            //std::cout << PlayerNumber << std::endl;

            if (pPlayerPawn_Athena_C_Index.size() != PlayerNumber) {
                SDK::APlayerPawn_Athena_C* pPlayerPawn_Athena_C_Temp = static_cast<SDK::APlayerPawn_Athena_C*>(Util::FindActor(SDK::APlayerPawn_Athena_C::StaticClass(), PlayerNumber));
                if (!pPlayerPawn_Athena_C_Temp)
                    printf("Finding PlayerPawn_Athena_C has failed, bailing-out immediately!\n");
                else {
                    // Find our SkeletalMesh in UObject cache.
                    auto pSkeletalMesh = SDK::UObject::FindObject<SDK::USkeletalMesh>(Core::SKELETAL_MESH);
                    if (pSkeletalMesh == nullptr)
                        printf("Finding SkeletalMesh has failed, bailing-out immediately!\n");
                    else {
                        pPlayerPawn_Athena_C_Temp->Mesh->SetSkeletalMesh(pSkeletalMesh, true);
                    }
                }

                pPlayerPawn_Athena_C_Index.push_back(pPlayerPawn_Athena_C_Temp);
            }
            
            if (pPlayerPawn_Athena_C_Index[PlayerNumber]) {
                //pPlayerPawn_Athena_C2->K2_TeleportTo(location, rotation);
                pPlayerPawn_Athena_C_Index[PlayerNumber]->K2_SetActorLocationAndRotation(location, rotation, false, false, new SDK::FHitResult());
                //pPlayerPawn_Athena_C2->Mesh->K2_SetWorldLocationAndRotation(location, rotation, false, true, new SDK::FHitResult());
            }
        }

        // Update thread only runs at 60hz, so we don't rape CPUs.
        Sleep(1000 / 60);
    }

    // Close socket
    closesocket(ConnectSocket);
    WSACleanup();

    return NULL;
}

DWORD WINAPI Main(LPVOID lpParam) {
    Util::InitConsole();

    printf("Aurora: Time Machine by Cyuubi, with help from others.\n");
    printf("Credits: Cendence, Irma, Kanner, Pivot, Samicc and Taj.\n\n");

    printf("Thank you all for helping, this wouldn't have been possible without you!\n");

    Util::InitSdk();
    Util::InitCore();

    auto pProcessEventAddress = Util::FindPattern("\x40\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8D\x6C\x24\x00\x48\x89\x9D\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC5\x48\x89\x85\x00\x00\x00\x00\x48\x63\x41\x0C", "xxxxxxxxxxxxxxx????xxxx?xxx????xxx????xxxxxx????xxxx");
    if (!pProcessEventAddress) {
        MessageBox(NULL, static_cast<LPCWSTR>(L"Finding pattern for ProcessEvent has failed, please re-open Fortnite and try again!"), static_cast<LPCWSTR>(L"Error"), MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }

    MH_CreateHook(static_cast<LPVOID>(pProcessEventAddress), ProcessEventHook, reinterpret_cast<LPVOID*>(&ProcessEvent));
    MH_EnableHook(static_cast<LPVOID>(pProcessEventAddress));

    // Find our PlayerPawn.
    pPlayerPawn_Athena_C = static_cast<SDK::APlayerPawn_Athena_C*>(Util::FindActor(SDK::APlayerPawn_Athena_C::StaticClass()));
    if (!pPlayerPawn_Athena_C)
        printf("Finding PlayerPawn_Athena_C has failed, bailing-out immediately!\n");
    else {
        // Find our SkeletalMesh in UObject cache.
        auto pSkeletalMesh = SDK::UObject::FindObject<SDK::USkeletalMesh>(Core::SKELETAL_MESH);
        if (!pSkeletalMesh)
            printf("Finding SkeletalMesh has failed, bailing-out immediately!\n");
        else {
            pPlayerPawn_Athena_C->Mesh->SetSkeletalMesh(pSkeletalMesh, true);

            Util::Possess(pPlayerPawn_Athena_C); // Possess our PlayerPawn.

            CreateThread(0, 0, UpdateThread, 0, 0, 0); // Create thread to handle input, etc...
            CreateThread(0, 0, ServerThread, 0, 0, 0); // Create thread to handle server updates

            Sleep(2000); // Wait for everything to be ready.

            // Tell the client that we are ready to start the match, this allows the loading screen to drop.
            static_cast<SDK::AAthena_PlayerController_C*>(Core::pPlayerController)->ServerReadyToStartMatch();

            auto pAuthorityGameMode = static_cast<SDK::AFortGameModeAthena*>((*Core::pWorld)->AuthorityGameMode);
            pAuthorityGameMode->StartMatch();
        }
    }

    return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH)
        CreateThread(0, 0, Main, hModule, 0, 0);

    return true;
}