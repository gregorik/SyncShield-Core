// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SyncShieldSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "SyncShield"))
class SYNCSHIELD_API USyncShieldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USyncShieldSettings();

	UPROPERTY(EditAnywhere, config, Category = "Status", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DirtyCheckIntervalSeconds;

	UPROPERTY(EditAnywhere, config, Category = "Source Control", meta = (ClampMin = "1.0", UIMin = "1.0", DisplayName = "Status Poll Interval (Seconds)"))
	float GitCheckIntervalSeconds;

	UPROPERTY(EditAnywhere, config, Category = "Source Control", meta = (DisplayName = "Auto Fetch (Git Only)"))
	bool bAutoFetch;

	UPROPERTY(EditAnywhere, config, Category = "Source Control", meta = (ClampMin = "10.0", UIMin = "10.0", EditCondition = "bAutoFetch", DisplayName = "Auto Fetch Interval (Seconds, Git Only)"))
	float AutoFetchIntervalSeconds;

	UPROPERTY(EditAnywhere, config, Category = "Notifications")
	bool bToastOnStatusChange;

	UPROPERTY(EditAnywhere, config, Category = "Notifications", meta = (ClampMin = "0.5", UIMin = "0.5"))
	float StatusToastMinIntervalSeconds;

	/** Absolute path to the Git executable. Leave empty to resolve "git" from PATH. */
	UPROPERTY(EditAnywhere, config, Category = "Source Control", meta = (DisplayName = "Git Executable Path"))
	FString GitExecutablePath;

	/** Absolute path to the Plastic SCM (Unity Version Control) CLI. Leave empty to resolve "cm" from PATH. */
	UPROPERTY(EditAnywhere, config, Category = "Source Control", meta = (DisplayName = "Plastic CLI Path"))
	FString PlasticExecutablePath;

	virtual FName GetCategoryName() const override
	{
		return FName("Plugins");
	}
};
