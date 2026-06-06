// Copyright (c) 2026 Mato Marion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "PropertyEditorDelegates.h"

class IDetailCustomization;

class FTextStringTableBrowserDetailCustomization : public IDetailCustomization
{
public:
	/**
	 * Bound to FPropertyEditorModule::GetGlobalRowExtensionDelegate().
	 * Only adds a button when the setting is ExtensionBar.
	 */
	static void OnGeneratePropertyRowExtension(
		const FOnGenerateGlobalRowExtensionArgs& InArgs,
		TArray<FPropertyRowExtensionButton>& OutExtensionButtons
	);

};

