#include "Attributes/CSGameplayAttributeCustomization.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilities/Public/AttributeSet.h"
#include "UnrealSharpGAS/Public/Attributes/CSGameplayAttributeSubsystem.h"
#include "UnrealSharpGAS/Public/CSAttributeSet.h"
#include "UnrealSharpGASEditor.h"
#include "Types/CSClass.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FCSGameplayAttributeCustomization"

TSharedRef<IPropertyTypeCustomization> FCSGameplayAttributeCustomization::MakeInstance()
{
	return MakeShareable(new FCSGameplayAttributeCustomization());
}

void FCSGameplayAttributeCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	// Get child handles for the three fields inside FGameplayAttribute:
	//   - Attribute      (TFieldPath<FProperty>)  — private UPROPERTY
	//   - AttributeOwner (TObjectPtr<UStruct>)    — private UPROPERTY
	//   - AttributeName  (FString)                — public UPROPERTY
	//
	// NOTE: GET_MEMBER_NAME_CHECKED cannot be used for 'Attribute' and 'AttributeOwner'
	// because they are private members (only engine's FAttributePropertyDetails is a friend).
	// We use the string literal instead; they are UPROPERTY() so the reflection system
	// resolves them correctly regardless of access level.
	AttributePropertyHandle = StructPropertyHandle->GetChildHandle("Attribute");
	OwnerPropertyHandle = StructPropertyHandle->GetChildHandle("AttributeOwner");
	NamePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayAttribute, AttributeName));

	check(AttributePropertyHandle.IsValid());
	check(OwnerPropertyHandle.IsValid());
	check(NamePropertyHandle.IsValid());

	// Build the initial attribute list
	RefreshAttributeList();
	CurrentEntry = FindCurrentEntry();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(300.0f)
		.MaxDesiredWidth(400.0f)
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FCSGameplayAttributeCustomization::CreateAttributeMenuContent)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FCSGameplayAttributeCustomization::GetDisplayValueText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FCSGameplayAttributeCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Don't show children — the header widget is the entire UI.
}

FText FCSGameplayAttributeCustomization::GetDisplayValueText() const
{
	if (CurrentEntry.IsValid())
	{
		return FText::FromString(CurrentEntry->DisplayString);
	}

	// Fall back to reading raw property values
	FString CurrentAttributeNameStr;
	FProperty* CurrentProperty = nullptr;

	if (NamePropertyHandle.IsValid())
	{
		NamePropertyHandle->GetValue(CurrentAttributeNameStr);
	}

	if (AttributePropertyHandle.IsValid())
	{
		FProperty* ObjPtr = nullptr;
		AttributePropertyHandle->GetValue(ObjPtr);
		CurrentProperty = ObjPtr;
	}

	if (CurrentProperty)
	{
		return FText::Format(LOCTEXT("CurrentAttribute", "{0}.{1}"),
			FText::FromString(CurrentProperty->GetOwnerVariant().GetName()),
			FText::FromString(CurrentProperty->GetName()));
	}

	if (!CurrentAttributeNameStr.IsEmpty())
	{
		return FText::FromString(CurrentAttributeNameStr);
	}

	return LOCTEXT("None", "None");
}

TSharedRef<SWidget> FCSGameplayAttributeCustomization::CreateAttributeMenuContent()
{
	RefreshAttributeList();

	return SNew(SBox)
		.MaxDesiredHeight(400.0f)
		[
			SNew(SListView<TSharedPtr<FAttributeEntry>>)
			.ListItemsSource(&AllAttributes)
			.OnGenerateRow(this, &FCSGameplayAttributeCustomization::OnGenerateRow)
			.OnSelectionChanged(this, &FCSGameplayAttributeCustomization::OnAttributeSelected)
			.SelectionMode(ESelectionMode::Single)
		];
}

void FCSGameplayAttributeCustomization::RefreshAttributeList()
{
	AllAttributes.Empty();
	TSet<FString> SeenKeys;

	// ---------------------------------------------------------------
	// Step 1: Engine baseline via FGameplayAttribute::GetAllAttributeProperties().
	// This gives us:
	//   - All native C++ UAttributeSet subclasses with proper inheritance
	//   - UAbilitySystemComponent system attributes (OutgoingDuration, IncomingDuration)
	//     which are tagged with SystemGameplayAttribute metadata
	//   - The function uses IncludeSuper so inherited properties are included
	//
	// It will NOT include C# managed (UCSClass) types because of the
	// hardcoded #if WITH_EDITORONLY_DATA ClassGeneratedBy filter inside.
	// ---------------------------------------------------------------
	TArray<FProperty*> EngineProperties;
	FGameplayAttribute::GetAllAttributeProperties(EngineProperties, FString(), true);

	for (FProperty* Prop : EngineProperties)
	{
		if (!Prop)
		{
			continue;
		}

		UClass* OwnerClass = Prop->GetOwnerClass();
		if (!OwnerClass)
		{
			continue;
		}

		FString ClassName = OwnerClass->GetName();
		// Strip _C suffix for consistent naming (BlueprintGeneratedClass convention)
		if (ClassName.EndsWith(TEXT("_C")))
		{
			ClassName.LeftChopInline(2);
		}
		FString PropName = Prop->GetName();
		FString Key = ClassName + TEXT(".") + PropName;

		if (!SeenKeys.Contains(Key))
		{
			SeenKeys.Add(Key);
			AllAttributes.Add(MakeShared<FAttributeEntry>(ClassName, PropName, Prop));
		}
	}

	// ---------------------------------------------------------------
	// Step 2: Merge C# managed (UCSClass) AttributeSet properties.
	// The engine function skips these because they have ClassGeneratedBy set.
	// We enumerate them manually using FGameplayAttribute::IsSupportedProperty logic.
	// ---------------------------------------------------------------
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class || !Class->IsChildOf(UAttributeSet::StaticClass()))
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		// Skip skeleton/reinstancing classes (Blueprint compilation artifacts)
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		// Only process C# managed classes here; engine step 1 already
		// handled native C++ and non-UCSClass Blueprint classes.
		// UCSClass inherits from UBlueprintGeneratedClass. UClass::IsA is private
		// (generated by UHT macro), so explicitly upcast to UObject* for the check.
		if (!static_cast<const UObject*>(Class)->IsA(UCSClass::StaticClass()))
		{
			continue;
		}

		// Strip _C suffix for consistent naming (BlueprintGeneratedClass convention).
		FString ClassName = Class->GetName();
		if (ClassName.EndsWith(TEXT("_C")))
		{
			ClassName.LeftChopInline(2);
		}

		// Use ExcludeSuper because base class (native C++ UAttributeSet)
		// properties are already in the engine baseline.
		for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (!Property)
			{
				continue;
			}

			bool bIsValid = false;
			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp && StructProp->Struct && StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
			{
				bIsValid = true;
			}
			else if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
			{
				bIsValid = NumericProp->IsFloatingPoint();
			}

			if (!bIsValid)
			{
				continue;
			}

			FString PropName = Property->GetName();
			FString Key = ClassName + TEXT(".") + PropName;

			if (!SeenKeys.Contains(Key))
			{
				SeenKeys.Add(Key);
				AllAttributes.Add(MakeShared<FAttributeEntry>(ClassName, PropName, Property));
			}
		}
	}

	// ---------------------------------------------------------------
	// 3) Sort alphabetically by display string.
	// ---------------------------------------------------------------
	AllAttributes.Sort([](const TSharedPtr<FAttributeEntry>& A, const TSharedPtr<FAttributeEntry>& B)
	{
		return A->DisplayString < B->DisplayString;
	});

	// ---------------------------------------------------------------
	// 4) Prepend "None" as the first option (matching engine behavior).
	// ---------------------------------------------------------------
	AllAttributes.Insert(MakeShared<FAttributeEntry>(TEXT(""), TEXT("None")), 0);

	UE_LOG(LogUnrealSharpGASEditor, Log, TEXT("CSGameplayAttributeCustomization: Refreshed attribute list with %d entries (engine + C#)"), AllAttributes.Num());
}

TSharedRef<ITableRow> FCSGameplayAttributeCustomization::OnGenerateRow(
	TSharedPtr<FAttributeEntry> Entry,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FAttributeEntry>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->DisplayString))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Margin(FMargin(4.0f, 2.0f))
		];
}

void FCSGameplayAttributeCustomization::OnAttributeSelected(
	TSharedPtr<FAttributeEntry> SelectedEntry,
	ESelectInfo::Type SelectInfo)
{
	if (!SelectedEntry.IsValid())
	{
		return;
	}

	CurrentEntry = SelectedEntry;
	SetAttributeFromEntry(SelectedEntry);
}

void FCSGameplayAttributeCustomization::SetAttributeFromEntry(const TSharedPtr<FAttributeEntry>& Entry)
{
	if (!Entry.IsValid())
	{
		return;
	}

	// Handle "None" selection: clear all fields
	if (Entry->ClassName.IsEmpty() || Entry->PropertyName == TEXT("None"))
	{
		AttributePropertyHandle->SetValue((FProperty*)nullptr);
		if (OwnerPropertyHandle.IsValid())
		{
			OwnerPropertyHandle->SetValue((UStruct*)nullptr);
		}
		if (NamePropertyHandle.IsValid())
		{
			NamePropertyHandle->SetValue(FString());
		}
		StructPropertyHandle->NotifyFinishedChangingProperties();
		return;
	}

	// Use stored FProperty* pointer directly if available (most reliable).
	FProperty* FoundProperty = Entry->PropertyPtr;
	if (!FoundProperty)
	{
		// Fallback: resolve the UClass by name.
		// For UCSClass (C# managed) types, the native UClass may have a "_C" suffix.
		UClass* FoundClass = FindObject<UClass>(nullptr, *Entry->ClassName);
		if (!FoundClass)
		{
			FString ClassNameWithSuffix = Entry->ClassName + TEXT("_C");
			FoundClass = FindObject<UClass>(nullptr, *ClassNameWithSuffix);
		}
		if (FoundClass)
		{
			FoundProperty = FindFProperty<FProperty>(FoundClass, *Entry->PropertyName);
		}
	}

	if (!FoundProperty)
	{
		UE_LOG(LogUnrealSharpGASEditor, Warning,
			TEXT("CSGameplayAttributeCustomization: Could not find property '%s' for class '%s'."),
			*Entry->PropertyName, *Entry->ClassName);
		return;
	}

	// Set Attribute (TFieldPath<FProperty>)
	AttributePropertyHandle->SetValue(FoundProperty);

	// Set AttributeOwner (TObjectPtr<UStruct>)
	if (OwnerPropertyHandle.IsValid())
	{
		OwnerPropertyHandle->SetValue(FoundProperty->GetOwnerStruct());
	}

	// Set AttributeName (FString)
	if (NamePropertyHandle.IsValid())
	{
		FString AttributeNameStr;
		FoundProperty->GetName(AttributeNameStr);
		NamePropertyHandle->SetValue(AttributeNameStr);
	}

	// Notify the property system that the change is complete
	StructPropertyHandle->NotifyFinishedChangingProperties();
}

TSharedPtr<FCSGameplayAttributeCustomization::FAttributeEntry> FCSGameplayAttributeCustomization::FindCurrentEntry() const
{
	FString CurrentAttributeNameStr;
	FProperty* CurrentProperty = nullptr;

	if (NamePropertyHandle.IsValid())
	{
		NamePropertyHandle->GetValue(CurrentAttributeNameStr);
	}

	if (AttributePropertyHandle.IsValid())
	{
		FProperty* ObjPtr = nullptr;
		AttributePropertyHandle->GetValue(ObjPtr);
		CurrentProperty = ObjPtr;
	}

	// If there's no property set, and name is empty, it's "None"
	if (!CurrentProperty && CurrentAttributeNameStr.IsEmpty())
	{
		// Return the "None" entry if available
		if (AllAttributes.Num() > 0)
		{
			return AllAttributes[0];
		}
		return nullptr;
	}

	if (CurrentProperty)
	{
		FString ClassName = CurrentProperty->GetOwnerVariant().GetName();
		FString PropName = CurrentProperty->GetName();

		// For UCSClass (BlueprintGeneratedClass), the native class name may have
		// a "_C" suffix while our entries store the canonical name without it.
		// Match both variants.
		FString ClassNameNoSuffix = ClassName;
		if (ClassNameNoSuffix.EndsWith(TEXT("_C")))
		{
			ClassNameNoSuffix.LeftChopInline(2);
		}

		// Try exact name first, then stripped name
		FString TargetKey = ClassName + TEXT(".") + PropName;
		FString TargetKeyNoSuffix = ClassNameNoSuffix + TEXT(".") + PropName;

		for (const TSharedPtr<FAttributeEntry>& Entry : AllAttributes)
		{
			if (Entry->DisplayString == TargetKey || Entry->DisplayString == TargetKeyNoSuffix)
			{
				return Entry;
			}
		}
	}

	if (!CurrentAttributeNameStr.IsEmpty() && CurrentAttributeNameStr.Contains(TEXT(".")))
	{
		// Fallback: try matching by the AttributeName string directly
		for (const TSharedPtr<FAttributeEntry>& Entry : AllAttributes)
		{
			if (Entry->DisplayString == CurrentAttributeNameStr)
			{
				return Entry;
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
