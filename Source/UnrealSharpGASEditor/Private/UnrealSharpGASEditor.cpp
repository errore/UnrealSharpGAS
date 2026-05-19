#include "UnrealSharpGASEditor.h"
#include "PropertyEditorModule.h"
#include "Attributes/CSGameplayAttributeCustomization.h"

#define LOCTEXT_NAMESPACE "FUnrealSharpGASEditorModule"

DEFINE_LOG_CATEGORY(LogUnrealSharpGASEditor);

void FUnrealSharpGASEditorModule::StartupModule()
{
    // Register custom property type layout for FGameplayAttribute.
    // The engine's default FAttributePropertyDetails in GameplayAbilitiesEditor
    // uses FGameplayAttribute::GetAllAttributeProperties() which has a hardcoded
    // #if WITH_EDITORONLY_DATA filter that skips ClassGeneratedBy classes.
    // Since UnrealSharp sets ClassGeneratedBy on all C# [UClass] types,
    // C# AttributeSet classes are invisible in the standard dropdown.
    //
    // This customization merges engine-standard attributes with those cached
    // by UCSGameplayAttributeSubsystem (which correctly handles UCSClass types).
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomPropertyTypeLayout(
        "GameplayAttribute",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCSGameplayAttributeCustomization::MakeInstance)
    );
}

void FUnrealSharpGASEditorModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomPropertyTypeLayout("GameplayAttribute");
    }
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FUnrealSharpGASEditorModule, UnrealSharpGASEditor)