#ifndef CORE_H
#define CORE_H

#include "SDK.hpp"

#include <Windows.h>
#include <stdio.h>

namespace Core {
	const std::string SKELETAL_MESH = "SkeletalMesh F_SML_Starter_Epic.F_SML_Starter_Epic"; // Here to make Irma happy, IDGAF.

	SDK::UWorld** pWorld;
	SDK::ULevel* pLevel;

	SDK::UGameInstance* pGameInstance;

	SDK::TArray<SDK::ULocalPlayer*> pLocalPlayers;
	SDK::ULocalPlayer* pLocalPlayer;

	SDK::TArray<SDK::AActor*>* pActors;

	SDK::APlayerController* pPlayerController;
};

#endif // CORE_H
