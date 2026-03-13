// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncShieldModule.h"
#include "SyncShieldStyle.h"
#include "SSyncShieldToolbar.h"

#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FSyncShieldModule"

FSyncShieldModule& FSyncShieldModule::Get()
{
	return FModuleManager::LoadModuleChecked<FSyncShieldModule>("SyncShield");
}

bool FSyncShieldModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("SyncShield");
}

FSyncShieldModule::~FSyncShieldModule() = default;

void FSyncShieldModule::StartupModule()
{
	FSyncShieldStyle::Initialize();
	FSyncShieldStyle::ReloadTextures();

	if (UToolMenus::Get())
	{
		UToolMenus::Get()->RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSyncShieldModule::RegisterMenus)
		);
	}

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetSubsystem->OnAssetEditorOpened().AddRaw(this, &FSyncShieldModule::OnAssetOpened);
		}
	}
}

void FSyncShieldModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetSubsystem->OnAssetEditorOpened().RemoveAll(this);
		}
	}

	FSyncShieldStyle::Shutdown();
}

void FSyncShieldModule::RegisterMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	if (Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("SyncShieldControls");
		UToolMenus::Get()->RemoveEntry("LevelEditor.LevelEditorToolBar.User", "SyncShieldControls", "SyncShieldStatusWidget");
		FToolMenuEntry Entry = FToolMenuEntry::InitWidget(
			"SyncShieldStatusWidget",
			SNew(SSyncShieldToolbar),
			LOCTEXT("SyncShieldLabel", "SyncShield"),
			true
		);
		Section.AddEntry(Entry);
	}
}

void FSyncShieldModule::OnAssetOpened(UObject* Asset)
{
	if (!Asset) return;

	LastActiveAsset = Asset;

	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (!SCModule.IsEnabled()) return;

	ISourceControlProvider& Provider = SCModule.GetProvider();
	FSourceControlStatePtr State = Provider.GetState(Asset->GetPackage(), EStateCacheUsage::Use);

	if (State.IsValid() && State->IsCheckedOutOther())
	{
		FString WhoIsIt = State->GetDisplayTooltip().ToString();

		FNotificationInfo Info(FText::Format(LOCTEXT("LockedToast", "LOCKED by Teammate:\n{0}"), FText::FromString(WhoIsIt)));
		Info.ExpireDuration = 5.0f;
		Info.Image = FAppStyle::GetBrush("Icons.WarningWithColor");
		Info.bUseLargeFont = true;

		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

bool FSyncShieldModule::SaveAllDirtyPackages(FString& OutSummary)
{
	TArray<UPackage*> Packages;
	FEditorFileUtils::GetDirtyPackages(Packages);

	if (Packages.Num() == 0)
	{
		OutSummary = TEXT("SyncShield found no packages to save.");
		return false;
	}

	TArray<UPackage*> PackagesToSave;
	TArray<FString> LockedFiles;

	if (ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

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
			}
		}

		if (LockedFiles.Num() > 0)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::FromString(FString::Printf(
					TEXT("The following files are locked by another user and will be blocked from saving:\n\n%s"),
					*FString::Join(LockedFiles, TEXT("\n")))));
		}

		if (PackagesToSave.Num() > 0)
		{
			TArray<FString> PackagesToCheckout;
			for (UPackage* Pkg : PackagesToSave)
			{
				PackagesToCheckout.Add(Pkg->GetName());
			}
			Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), PackagesToCheckout);
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
			: TEXT("SyncShield found nothing saveable.");
		return false;
	}

	const FEditorFileUtils::EPromptReturnCode SaveResult = FEditorFileUtils::PromptForCheckoutAndSave(
		PackagesToSave,
		false,
		true,
		nullptr,
		false,
		true);

	bool bSaved = false;
	if (SaveResult == FEditorFileUtils::PR_Success)
	{
		bSaved = true;
		OutSummary = FString::Printf(TEXT("SyncShield successfully saved %d package(s)."), PackagesToSave.Num());
	}
	else if (SaveResult == FEditorFileUtils::PR_Cancelled)
	{
		OutSummary = TEXT("SyncShield was cancelled by the user.");
	}
	else // PR_Failure
	{
		OutSummary = TEXT("SyncShield encountered errors during checkout/save.");
	}

	if (LockedFiles.Num() > 0)
	{
		OutSummary += FString::Printf(TEXT("\nBlocked %d locked package(s)."), LockedFiles.Num());
	}

	return bSaved;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSyncShieldModule, SyncShield)


