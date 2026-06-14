// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealSharpGAS : ModuleRules
{
	public UnrealSharpGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"GameplayAbilities",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"GameplayTasks", 
				"UnrealSharpCore",
				"UnrealSharpBinds"
			}
			);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"BlueprintGraph",
				"UnrealEd",
				"KismetCompiler",
				"Kismet",
			});
		}
		
		PublicDefinitions.Add("FlattenGlue=1");
		PublicDefinitions.Add("ExtendModule=GameplayAbilities");
	}
}
