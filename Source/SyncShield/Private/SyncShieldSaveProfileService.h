// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UPackage;
class UWorld;

class FSyncShieldSaveProfileService
{
public:

	/**
	 * True when a package belongs to the given level: the level package itself, or one
	 * of its World Partition external actor/object packages.
	 */
	static bool IsPackageInLevelScope(const FString& PackageName, const FString& WorldPackageName);

	bool SaveAllDirtyPackages(FString& OutSummary);
	bool SaveDirtyBlueprintPackages(FString& OutSummary);
	bool SaveCurrentLevelPackages(FString& OutSummary);
	FString GetLastSummary() const;

private:
	enum class ESaveProfile : uint8
	{
		AllDirty,
		DirtyBlueprints,
		CurrentLevel
	};

	bool SavePackagesForProfile(ESaveProfile Profile, FString& OutSummary);
	TArray<UPackage*> CollectPackagesForProfile(ESaveProfile Profile) const;
	bool IsBlueprintPackage(UPackage* Package) const;
	bool IsCurrentLevelPackage(UPackage* Package, const UWorld* CurrentWorld) const;
	bool SavePackages(TArray<UPackage*> Packages, const TCHAR* ProfileLabel, FString& OutSummary);
	void RecordSummary(const FString& Summary);

	mutable FCriticalSection StateGuard;
	FString LastSummary;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSyncShieldServiceTestAccessor;
#endif
};
