#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Custom property type customization for FGameplayAttribute.
 *
 * The engine's default FAttributePropertyDetails in GameplayAbilitiesEditor
 * uses FGameplayAttribute::GetAllAttributeProperties() which has a hardcoded
 * #if WITH_EDITORONLY_DATA filter that skips ClassGeneratedBy classes.
 * Since UnrealSharp sets ClassGeneratedBy via UCSClass::SetOwningBlueprint(),
 * all C# [UClass] AttributeSet classes are invisible in the standard dropdown.
 *
 * This customization replaces the engine's default, enumerating attributes
 * from BOTH engine's GetAllAttributeProperties() AND the C#-aware
 * UCSGameplayAttributeSubsystem cache, then merging the results.
 */
class FCSGameplayAttributeCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/** Data for a single entry in the attribute picker menu. */
	struct FAttributeEntry
	{
		FString ClassName;
		FString PropertyName;
		FString DisplayString; // "ClassName.PropertyName"
		FProperty* PropertyPtr; // Direct FProperty* pointer (for reliable setting)

		FAttributeEntry(const FString& InClass, const FString& InProp, FProperty* InProperty = nullptr)
			: ClassName(InClass)
			, PropertyName(InProp)
			, DisplayString(InClass + TEXT(".") + InProp)
			, PropertyPtr(InProperty)
		{}

		FAttributeEntry()
			: PropertyPtr(nullptr)
		{}
	};

	/** Build the display text for the currently selected attribute. */
	FText GetDisplayValueText() const;

	/** Create the dropdown menu widget for picking an attribute. */
	TSharedRef<SWidget> CreateAttributeMenuContent();

	/** Rebuild the list of all available attributes (engine + C#). */
	void RefreshAttributeList();

	/** Generate a row widget for an attribute entry. */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FAttributeEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called when user selects an entry from the menu. */
	void OnAttributeSelected(TSharedPtr<FAttributeEntry> SelectedEntry, ESelectInfo::Type SelectInfo);

	/** Get the entry matching the current property values (if any). */
	TSharedPtr<FAttributeEntry> FindCurrentEntry() const;

	/** Resolve an FProperty* from ClassName.PropertyName and set all child handles. */
	void SetAttributeFromEntry(const TSharedPtr<FAttributeEntry>& Entry);

	/** Owning struct property handle. */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Child handles for the three fields inside FGameplayAttribute. */
	TSharedPtr<IPropertyHandle> AttributePropertyHandle;   // TFieldPath<FProperty> Attribute
	TSharedPtr<IPropertyHandle> OwnerPropertyHandle;        // TObjectPtr<UStruct> AttributeOwner
	TSharedPtr<IPropertyHandle> NamePropertyHandle;         // FString AttributeName

	/** All available attributes, merged from engine + subsystem. */
	TArray<TSharedPtr<FAttributeEntry>> AllAttributes;

	/** Cached current selection. */
	TSharedPtr<FAttributeEntry> CurrentEntry;
};
