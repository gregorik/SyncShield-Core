// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UObject;

class FSyncShieldModule : public IModuleInterface
{
public:
	static FSyncShieldModule& Get();
	static bool IsAvailable();
	virtual ~FSyncShieldModule() override;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool SaveAllDirtyPackages(FString& OutSummary);

private:
	/** Registers the SyncShield status widget into the main Level Editor Toolbar. */
	void RegisterMenus();
	void OnAssetOpened(UObject* Asset);

	TWeakObjectPtr<UObject> LastActiveAsset;
};

