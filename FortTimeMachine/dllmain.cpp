#include "SDK.hpp"

#include <Windows.h>
#include <stdio.h>

#include <MinHook.h>
#pragma comment(lib, "minhook.lib")

#include "core.h"
#include "util.h"

SDK::AFortPlayerPawn* pFortPlayerPawn;

PVOID(*ProcessEvent)(SDK::UObject*, SDK::UFunction*, PVOID) = nullptr;

PVOID ProcessEventHook(SDK::UObject* pObject, SDK::UFunction* pFunction, PVOID pParams) {
    if (pObject && pFunction) {
        // HACK: This will probably cause a crash, but it's worth a try.
        if (pFunction->GetName().find("StopHoldProgress") != std::string::npos)
            return NULL;
    }

    return ProcessEvent(pObject, pFunction, pParams);
}

DWORD UpdateThread(LPVOID lpParam) {
    while (1) {
        // Keybind to jump (only run if not skydiving, might need to fix this more though):
        if (GetKeyState(VK_SPACE) & 0x8000 && pFortPlayerPawn) {
            if (!pFortPlayerPawn->IsJumpProvidingForce())
                pFortPlayerPawn->Jump();
        }

        // Keybind to sprint (only run if not skydiving):
        if (GetKeyState(VK_SHIFT) & 0x8000 && pFortPlayerPawn)
            pFortPlayerPawn->CurrentMovementStyle = SDK::EFortMovementStyle::Sprinting;
        else if (pFortPlayerPawn)
            pFortPlayerPawn->CurrentMovementStyle = SDK::EFortMovementStyle::Running;

        // Keybind to equip weapon
        if (GetKeyState(VK_END) & 0x8000 && pFortPlayerPawn) {
            auto pFortWeapon = static_cast<SDK::AFortWeapon*>(Util::FindActor(SDK::AFortWeapon::StaticClass()));
            if (!pFortWeapon)
                printf("Finding FortWeapon has failed, bailing-out immediately!\n");
            else {
                pFortWeapon->ClientGivenTo(pFortPlayerPawn);

                pFortPlayerPawn->ClientInternalEquipWeapon(pFortWeapon);
                pFortPlayerPawn->ServerInternalEquipWeapon(pFortWeapon);
            }
        }
		
		// Reloading weapon
		if (GetKeyState(0x52) & 0x8000 && pFortPlayerPawn) {
			auto pFortWeapon = static_cast<SDK::AFortWeapon*>(Util::FindActor(SDK::AFortWeapon::StaticClass()));
			if (!pFortWeapon)
				printf("Cannot reload without a valid weapon.\n");
			else if (!pFortWeapon->IsReloading() && (pFortWeapon->GetRemainingAmmo() != pFortWeapon->GetBulletsPerClip()))
			{
				pFortWeapon->Reload();
				pFortWeapon->PlayReloadFX(SDK::EFortReloadFXState::ReloadStart);
			}	
		}

		// Firing weapon (Primary fire)
		if (GetKeyState(VK_LBUTTON) & 0x8000 && pFortPlayerPawn) {
			auto pFortWeapon = static_cast<SDK::AFortWeapon*>(Util::FindActor(SDK::AFortWeapon::StaticClass()));
			if (!pFortWeapon)
				printf("Cannot shoot without a valid weapon.\n");
			else if (!pFortWeapon->IsReloading() && !(pFortWeapon->GetRemainingAmmo() != 0))
			{
				pFortWeapon->PlayWeaponFireFX(false);
			}
		}

        // Update thread only runs at 60hz, so we don't rape CPUs.
        Sleep(1000 / 60);
    }

    return NULL;
}

DWORD WINAPI Main(LPVOID lpParam) {
    Util::InitConsole();

    printf("Aurora: Time Machine by Cyuubi, with help from others.\n");
    printf("Credits: Cendence, Irma, Kanner, Pivot, Samicc and Taj.\n\n");
	printf("Experimental StW version by pivotman319\n");

    printf("Thank you all for helping, this wouldn't have been possible without you!\n");

    Util::InitSdk();
    Util::InitCore();

    auto pProcessEventAddress = Util::FindPattern("\x40\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8D\x6C\x24\x00\x48\x89\x9D\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC5\x48\x89\x85\x00\x00\x00\x00\x48\x63\x41\x0C", "xxxxxxxxxxxxxxx????xxxx?xxx????xxx????xxxxxx????xxxx");
    if (!pProcessEventAddress) {
        MessageBox(NULL, static_cast<LPCWSTR>(L"Finding pattern for ProcessEvent has failed. Please relaunch Fortnite and try again."), static_cast<LPCWSTR>(L"Error"), MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }

    MH_CreateHook(static_cast<LPVOID>(pProcessEventAddress), ProcessEventHook, reinterpret_cast<LPVOID*>(&ProcessEvent));
    MH_EnableHook(static_cast<LPVOID>(pProcessEventAddress));

    // Find our PlayerPawn.
    pFortPlayerPawn = static_cast<SDK::AFortPlayerPawn*>(Util::FindActor(SDK::AFortPlayerPawn::StaticClass()));
    if (!pFortPlayerPawn)
        printf("Finding FortPlayerPawn has failed, bailing-out immediately!\n");
    else {
		Util::Possess(pFortPlayerPawn); // Possess our PlayerPawn.
		CreateThread(0, 0, UpdateThread, 0, 0, 0); // Create thread to handle input, etc...

        Sleep(2000); // Wait for everything to be ready.
		
		// Tell the client that we are ready to start the match, this allows the loading screen to drop.
        static_cast<SDK::AFortPlayerController*>(Core::pPlayerController)->ServerReadyToStartMatch();

        auto pAuthorityGameMode = static_cast<SDK::AFortGameMode*>((*Core::pWorld)->AuthorityGameMode);
        pAuthorityGameMode->StartMatch();
    }

    return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH)
        CreateThread(0, 0, Main, hModule, 0, 0);

    return true;
}
