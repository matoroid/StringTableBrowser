// Copyright (c) 2026 Mato Marion. All Rights Reserved.

#include "FTextStringTableBrowserDetailCustomization.h"
#include "StringTableBrowserSettings.h"
#include "SStringTableBrowserPickerDropdown.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StringTableBrowserHelpers.h"
#include "StringTableBrowserTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "UObject/TextProperty.h"

#define LOCTEXT_NAMESPACE "FTextStringTableBrowserDetailCustomization"

// -------------------------------------------------------------------------
// Extension bar path
// -------------------------------------------------------------------------

void FTextStringTableBrowserDetailCustomization::OnGeneratePropertyRowExtension(
    const FOnGenerateGlobalRowExtensionArgs& InArgs,
    TArray<FPropertyRowExtensionButton>& OutExtensionButtons
)
{
    if (!InArgs.PropertyHandle.IsValid() || 
    	!InArgs.PropertyHandle->GetProperty() ||
        !InArgs.PropertyHandle->GetProperty()->IsA<FTextProperty>()
    )
    {
        return;
    }

	UE_LOG(LogStringTableBrowser, Verbose, TEXT("StringTableBrowser: ExtensionBar - Property: %s"), *InArgs.PropertyHandle->GetProperty()->GetName());

    TSharedPtr<IPropertyHandle> PropertyHandle = InArgs.PropertyHandle;
    TSharedPtr<FString> LastSearchText = MakeShared<FString>();
    {
        FText CurrentValue;
        PropertyHandle->GetValue(CurrentValue);
        *LastSearchText = CurrentValue.ToString();
    }

	FPropertyRowExtensionButton Button;
    Button.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), StringTableBrowserIcons::OpenBrowserSearch);
    Button.Label = LOCTEXT("SearchBtnLabel", "Search String Tables");
    Button.ToolTip = LOCTEXT("SearchBtnTooltip",
        "Search all String Tables and bind this FText property to the "
        "selected entry as a proper string table reference."
    );

    Button.UIAction = FUIAction(
        FExecuteAction::CreateLambda([PropertyHandle, LastSearchText]()
            {
                FStringTableBrowserHelpers::OpenPickerDropdown(PropertyHandle, LastSearchText);
            }
        ),
        FCanExecuteAction()
    );
	
	OutExtensionButtons.Insert(MoveTemp(Button), 0);
}

#undef LOCTEXT_NAMESPACE
