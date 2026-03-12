// Copyright (c) 2026 GregOrigin. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncShieldStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h" 

// Definition of the singleton
TSharedPtr<FSlateStyleSet> FSyncShieldStyle::StyleInstance = nullptr;

void FSyncShieldStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FSyncShieldStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

void FSyncShieldStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FSyncShieldStyle::Get()
{
	return *StyleInstance;
}

FName FSyncShieldStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("SyncShieldStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FSyncShieldStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	// 1. Locate the Resources folder within the Plugin directory
	// Result: .../Plugins/SyncShield/Resources/
	FString ContentDir = IPluginManager::Get().FindPlugin("SyncShield")->GetBaseDir() / TEXT("Resources");
	Style->SetContentRoot(ContentDir);

	// 2. Helper Lambda to make loading brushes cleaner
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	auto ImageBrush = [&](const FString& RelativePath, const FVector2D& ImageSize)
	{
		return new FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), ImageSize);
	};

	// 3. Register your Icons here
	// Assuming you have 'Icon128.png' in the Resources folder
	Style->Set("SyncShield.PluginIcon", ImageBrush(TEXT("Icon128"), Icon128x128));

	// Example: If you create custom traffic light icons later
	// Style->Set("SyncShield.Status.Green", ImageBrush(TEXT("Status_Green"), Icon16x16));
	// Style->Set("SyncShield.Status.Red",   ImageBrush(TEXT("Status_Red"),   Icon16x16));

	return Style;
}
