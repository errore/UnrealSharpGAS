#include "Attributes/CSGameplayAttributeSubsystem.h"
#include "AttributeSet.h"
#include "UnrealSharpGAS.h"
#include "Types/CSClass.h"

void UCSGameplayAttributeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CacheAllGameplayAttributes();
}

UCSGameplayAttributeSubsystem* UCSGameplayAttributeSubsystem::Get()
{
	if (!GEngine)
	{
		return nullptr;
	}
	
	return GEngine->GetEngineSubsystem<UCSGameplayAttributeSubsystem>();
}

FGameplayAttribute UCSGameplayAttributeSubsystem::FindGameplayAttributeByName(const FString& AttributeSetClassName, const FString& PropertyName)
{
	UCSGameplayAttributeSubsystem* Subsystem = Get();
	if (!Subsystem)
	{
		return FGameplayAttribute();
	}

	FString Key = FString::Printf(TEXT("%s.%s"), *AttributeSetClassName, *PropertyName);
	if (FGameplayAttribute* FoundAttribute = Subsystem->CachedAttributes.Find(Key))
	{
		return *FoundAttribute;
	}

	return FGameplayAttribute();
}

void UCSGameplayAttributeSubsystem::GetCachedAttributeNamesForClass(const FString& AttributeSetClassName, TArray<FString>& OutAttributeNames) const
{
	OutAttributeNames.Empty();

	// Strip _C suffix if present, since our cache keys always use the canonical name.
	FString NormalizedClassName = AttributeSetClassName;
	if (NormalizedClassName.EndsWith(TEXT("_C")))
	{
		NormalizedClassName.LeftChopInline(2);
	}

	FString ClassPrefix = NormalizedClassName + TEXT(".");

	for (const TTuple<FString, FGameplayAttribute>& Pair : CachedAttributes)
	{
		if (Pair.Key.StartsWith(ClassPrefix))
		{
			// Extract property name after "ClassName."
			FString PropertyName = Pair.Key.Mid(ClassPrefix.Len());
			OutAttributeNames.Add(PropertyName);
		}
	}
}

void UCSGameplayAttributeSubsystem::CacheAllGameplayAttributes()
{
	CachedAttributes.Empty();
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(UAttributeSet::StaticClass()))
		{
			continue;
		}
		
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		
		// Skip skeleton/reinstancing classes (Blueprint compilation artifacts)
		// SKEL_ prefix = skeleton class (duplicate used during compilation/reload)
		// REINST_ prefix = re-instancing stub class
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		
#if WITH_EDITOR
		// UCSClass (from UnrealSharp) sets ClassGeneratedBy = OwningBlueprint.
		// Allow C#-generated classes through; skip Blueprint-generated classes.
		// Use a clear check: if ClassGeneratedBy is set, only allow UCSClass types.
		if (Class->ClassGeneratedBy)
		{
			if (static_cast<const UObject*>(Class)->IsA(UCSClass::StaticClass()))
			{
				UE_LOG(LogUnrealSharpGAS, Verbose, TEXT("CacheAll: Allowing C# managed AttributeSet: %s"), *Class->GetName());
			}
			else
			{
				UE_LOG(LogUnrealSharpGAS, Verbose, TEXT("CacheAll: Skipping BP-generated AttributeSet: %s"), *Class->GetName());
				continue;
			}
		}
#endif
		
		TArray<FProperty*> AttributeProperties;
		GetAllAttributeProperties(Class, AttributeProperties);

		// Cache each attribute with "ClassName.PropertyName" as key
		for (FProperty* Property : AttributeProperties)
		{
			if (Property)
			{
				// Strip _C suffix for consistent naming in cache keys.
				// UCSClass (C# managed) uses BlueprintGeneratedClass naming, which
				// appends "_C" to the class name. Remove it so that lookups via
				// FindGameplayAttributeByName("CSBaseAttributeSet", "Health") work.
				FString ClassName = Class->GetName();
				if (ClassName.EndsWith(TEXT("_C")))
				{
					ClassName.LeftChopInline(2);
				}
				
				FString Key = FString::Printf(TEXT("%s.%s"), *ClassName, *Property->GetName());
				FGameplayAttribute Attribute(Property);
				CachedAttributes.Add(Key, Attribute);
			}
		}
	}

	UE_LOG(LogUnrealSharpGAS, Log, TEXT("GameplayAttributeSubsystem: Cached %d gameplay attributes"), CachedAttributes.Num());
}

void UCSGameplayAttributeSubsystem::ForceRefresh()
{
	CacheAllGameplayAttributes();
}

void UCSGameplayAttributeSubsystem::GetAllAttributeProperties(UClass* AttributeSetClass, TArray<FProperty*>& OutProperties)
{
	if (!AttributeSetClass)
	{
		return;
	}

	// NOTE: We CANNOT use FGameplayAttribute::GetAllAttributeProperties() here.
	// That engine function has a hardcoded #if WITH_EDITORONLY_DATA filter:
	//     if (!Class->ClassGeneratedBy) continue;
	// This is always active in editor builds regardless of the UseEditorOnlyData
	// parameter, which filters out ALL C# [UClass] generated AttributeSet classes.
	//
	// Instead, iterate the class's own properties directly and filter for
	// FGameplayAttributeData (or FCSGameplayAttributeData) struct properties
	// plus floating-point numeric properties (legacy support).
	for (TFieldIterator<FProperty> PropertyIt(AttributeSetClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property)
		{
			continue;
		}

		// FGameplayAttribute::IsSupportedProperty logic:
		// - FStructProperty whose struct inherits from FGameplayAttributeData
		// - FNumericProperty that is a floating-point type (legacy)
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp && StructProp->Struct && StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
		{
			OutProperties.Add(Property);
			continue;
		}

		const FNumericProperty* NumericProp = CastField<FNumericProperty>(Property);
		if (NumericProp && NumericProp->IsFloatingPoint())
		{
			OutProperties.Add(Property);
			continue;
		}
	}
}