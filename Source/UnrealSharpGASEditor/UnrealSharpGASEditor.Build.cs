using UnrealBuildTool;

public class UnrealSharpGASEditor : ModuleRules
{
    public UnrealSharpGASEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore", 
                "GameplayAbilities", 
                "GameplayAbilitiesEditor",
                "PropertyEditor",
                "UnrealSharpRuntimeGlue",
                "UnrealSharpCore",
                "UnrealEd",
                "UnrealSharpGAS",
                "BlueprintGraph"
            }
        );
        
        PublicDefinitions.Add("FlattenGlue=1");
    }
}
