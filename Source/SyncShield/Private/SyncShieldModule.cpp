// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncShieldModule.h"
#include "SyncShieldSaveProfileService.h"
#include "SyncShieldStyle.h"
#include "SyncShieldSettings.h"
#include "SSyncShieldToolbar.h"

#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

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
	SaveProfileService = MakeUnique<FSyncShieldSaveProfileService>();

	if (UToolMenus::Get())
	{
		UToolMenus::Get()->RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSyncShieldModule::RegisterMenus)
		);
	}

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Editor",
			"Plugins",
			"SyncShield",
			LOCTEXT("SyncShieldSettingsName", "SyncShield"),
			LOCTEXT("SyncShieldSettingsDescription", "Configure SyncShield source control status and automation settings."),
			GetMutableDefault<USyncShieldSettings>()
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

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "SyncShield");
	}

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetSubsystem->OnAssetEditorOpened().RemoveAll(this);
		}
	}

	SaveProfileService.Reset();

	FSyncShieldStyle::Shutdown();
}

void FSyncShieldModule::RegisterMenus()
{
	// Scope the entry to this module. Without an owner, FToolMenuEntry::InitWidget
	// records an empty owner and UnregisterOwner() on shutdown cannot remove it, so
	// the widget outlives the module and keeps calling into unloaded code.
	FToolMenuOwnerScoped OwnerScoped(this);

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
	return SaveProfileService ? SaveProfileService->SaveAllDirtyPackages(OutSummary) : false;
}

bool FSyncShieldModule::SaveDirtyBlueprintPackages(FString& OutSummary)
{
	return SaveProfileService ? SaveProfileService->SaveDirtyBlueprintPackages(OutSummary) : false;
}

bool FSyncShieldModule::SaveCurrentLevelPackages(FString& OutSummary)
{
	return SaveProfileService ? SaveProfileService->SaveCurrentLevelPackages(OutSummary) : false;
}

FString FSyncShieldModule::BuildSubsystemSummary() const
{
	TArray<FString> Lines;

	if (SaveProfileService)
	{
		Lines.Add(SaveProfileService->GetLastSummary());
	}

	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSyncShieldModule, SyncShield)
