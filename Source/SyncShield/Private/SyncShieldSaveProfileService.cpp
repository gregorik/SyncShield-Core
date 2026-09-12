// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "SyncShieldSaveProfileService.h"

#include "Editor/EditorEngine.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/PlatformFileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "UObject/Package.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "SyncShieldSaveProfiles"

bool FSyncShieldSaveProfileService::SaveAllDirtyPackages(FString& OutSummary)
{
	return SavePackagesForProfile(ESaveProfile::AllDirty, OutSummary);
}

bool FSyncShieldSaveProfileService::SaveDirtyBlueprintPackages(FString& OutSummary)
{
	return SavePackagesForProfile(ESaveProfile::DirtyBlueprints, OutSummary);
}

bool FSyncShieldSaveProfileService::SaveCurrentLevelPackages(FString& OutSummary)
{
	return SavePackagesForProfile(ESaveProfile::CurrentLevel, OutSummary);
}

FString FSyncShieldSaveProfileService::GetLastSummary() const
{
	FScopeLock Lock(&StateGuard);
	if (LastSummary.IsEmpty())
	{
		return TEXT("Profiles: idle");
	}
	return FString::Printf(TEXT("Profiles: %s"), *LastSummary);
}

bool FSyncShieldSaveProfileService::SavePackagesForProfile(ESaveProfile Profile, FString& OutSummary)
{
	const TCHAR* ProfileLabel = TEXT("save profile");
	switch (Profile)
	{
	case ESaveProfile::AllDirty:
		ProfileLabel = TEXT("all dirty packages");
		break;
	case ESaveProfile::DirtyBlueprints:
		ProfileLabel = TEXT("dirty Blueprints");
		break;
	case ESaveProfile::CurrentLevel:
		ProfileLabel = TEXT("current level");
		break;

	}

	TArray<UPackage*> Packages = CollectPackagesForProfile(Profile);
	return SavePackages(MoveTemp(Packages), ProfileLabel, OutSummary);
}

TArray<UPackage*> FSyncShieldSaveProfileService::CollectPackagesForProfile(ESaveProfile Profile) const
{
	TArray<UPackage*> Packages;

	switch (Profile)
	{
	case ESaveProfile::AllDirty:
		FEditorFileUtils::GetDirtyPackages(Packages);
		break;

	case ESaveProfile::DirtyBlueprints:
		FEditorFileUtils::GetDirtyContentPackages(Packages);
		Packages.RemoveAll([this](UPackage* Package) { return !IsBlueprintPackage(Package); });
		break;

	case ESaveProfile::CurrentLevel:
		{
			// Start from every dirty package, not just world packages: World Partition
			// external actor/object packages are content packages, so filtering
			// GetDirtyWorldPackages would never see them.
			FEditorFileUtils::GetDirtyPackages(Packages);
			const UWorld* CurrentWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			Packages.RemoveAll([this, CurrentWorld](UPackage* Package) { return !IsCurrentLevelPackage(Package, CurrentWorld); });
		}
		break;

	}

	Packages.RemoveAll([](UPackage* Package)
	{
		return !Package || !Package->IsDirty() || !FPackageName::IsValidLongPackageName(Package->GetName(), false);
	});

	return Packages;
}

bool FSyncShieldSaveProfileService::IsPackageInLevelScope(const FString& PackageName, const FString& WorldPackageName)
{
	if (PackageName.IsEmpty() || WorldPackageName.IsEmpty())
	{
		return false;
	}

	if (PackageName.Equals(WorldPackageName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	// World Partition stores actors and objects in sibling packages rather than in the
	// .umap, so a level save that only looks at world packages silently drops them.
	TArray<FString> ExternalRoots = ULevel::GetExternalActorsPaths(WorldPackageName);
	ExternalRoots.Append(ULevel::GetExternalObjectsPaths(WorldPackageName));

	for (const FString& Root : ExternalRoots)
	{
		if (!Root.IsEmpty() && PackageName.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool FSyncShieldSaveProfileService::IsBlueprintPackage(UPackage* Package) const
{
	if (!Package)
	{
		return false;
	}

	if (UObject* Asset = Package->FindAssetInPackage())
	{
		return Asset->IsA<UBlueprint>();
	}

	return false;
}

bool FSyncShieldSaveProfileService::IsCurrentLevelPackage(UPackage* Package, const UWorld* CurrentWorld) const
{
	if (!Package || !CurrentWorld)
	{
		return false;
	}

	if (Package == CurrentWorld->GetPackage())
	{
		return true;
	}

	const UPackage* WorldPackage = CurrentWorld->GetPackage();
	if (WorldPackage && IsPackageInLevelScope(Package->GetName(), WorldPackage->GetName()))
	{
		return true;
	}

	if (UObject* Asset = Package->FindAssetInPackage())
	{
		return Asset->GetTypedOuter<UWorld>() == CurrentWorld;
	}

	return false;
}

bool FSyncShieldSaveProfileService::SavePackages(TArray<UPackage*> Packages, const TCHAR* ProfileLabel, FString& OutSummary)
{
	if (Packages.Num() == 0)
	{
		OutSummary = FString::Printf(TEXT("SyncShield found no packages to save for %s."), ProfileLabel);
		RecordSummary(OutSummary);
		return false;
	}

	TArray<UPackage*> PackagesToSave;
	TArray<FString> LockedFiles;
	bool bAnyNeedsCheckout = false;
	const bool bSourceControlEnabled = ISourceControlModule::Get().IsEnabled();

	if (bSourceControlEnabled)
	{
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

		// Refresh state before reading it. EStateCacheUsage::Use returns nothing on a
		// cold cache, which would make every file look neither locked nor checkout-able
		// and defeat the lock check entirely.
		Provider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), Packages, EConcurrency::Synchronous);

		TArray<UPackage*> PackagesToCheckout;

		for (UPackage* Package : Packages)
		{
			FSourceControlStatePtr State = Provider.GetState(Package, EStateCacheUsage::Use);
			if (State.IsValid() && State->IsCheckedOutOther())
			{
				LockedFiles.Add(Package->GetName());
			}
			else
			{
				PackagesToSave.Add(Package);
				// Only attempt checkout for files that actually support it.
				// Plain Git without LFS locking returns CanCheckout() = false for all files,
				// so FCheckOut would be a silent no-op that leaves the checkout dialog broken.
				if (State.IsValid() && State->CanCheckout())
				{
					PackagesToCheckout.Add(Package);
					bAnyNeedsCheckout = true;
				}
			}
		}

		if (LockedFiles.Num() > 0)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("LockedBySomeoneElse", "These files are locked by another user and will not be saved:\n\n{0}"),
					FText::FromString(FString::Join(LockedFiles, TEXT("\n")))));
		}

		if (PackagesToCheckout.Num() > 0)
		{
			// Pass packages, not package names: the FString overload treats its input as
			// file paths, so "/Game/Foo/Bar" resolves to a file that does not exist and
			// the checkout silently does nothing.
			Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), PackagesToCheckout, EConcurrency::Synchronous);
		}
	}
	else
	{
		PackagesToSave = MoveTemp(Packages);
	}

	if (PackagesToSave.Num() == 0)
	{
		OutSummary = LockedFiles.Num() > 0
			? FString::Printf(TEXT("SyncShield blocked %d locked package(s)."), LockedFiles.Num())
			: FString::Printf(TEXT("SyncShield found nothing saveable for %s."), ProfileLabel);
		RecordSummary(OutSummary);
		return false;
	}

	// When the provider doesn't support checkout (e.g., plain Git without LFS locking),
	// skip the checkout dialog by treating packages as already checked out.
	// This prevents the "Check Out Selected" button from appearing disabled.
	const bool bAlreadyCheckedOut = !bSourceControlEnabled || !bAnyNeedsCheckout;

	const FEditorFileUtils::EPromptReturnCode SaveResult = FEditorFileUtils::PromptForCheckoutAndSave(
		PackagesToSave,
		false,
		true,
		nullptr,
		bAlreadyCheckedOut,
		true);

	bool bSaved = false;
	if (SaveResult == FEditorFileUtils::PR_Success)
	{
		bSaved = true;
		OutSummary = FString::Printf(TEXT("SyncShield successfully saved %d package(s) for %s."), PackagesToSave.Num(), ProfileLabel);
	}
	else if (SaveResult == FEditorFileUtils::PR_Cancelled)
	{
		OutSummary = FString::Printf(TEXT("SyncShield was cancelled by the user during %s."), ProfileLabel);
	}
	else // PR_Failure
	{
		OutSummary = FString::Printf(TEXT("SyncShield encountered errors during checkout/save for %s."), ProfileLabel);
	}

	if (LockedFiles.Num() > 0)
	{
		OutSummary += FString::Printf(TEXT("\nBlocked %d locked package(s)."), LockedFiles.Num());
	}

	RecordSummary(OutSummary);
	return bSaved;
}

void FSyncShieldSaveProfileService::RecordSummary(const FString& Summary)
{
	FScopeLock Lock(&StateGuard);
	LastSummary = Summary;
}

#undef LOCTEXT_NAMESPACE
