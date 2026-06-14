#include "Nodes/K2Node_CSAbilityTask.h"

#if WITH_EDITOR
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTask.h"
#include "Types/CSClass.h"
#include "Utilities/CSClassUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_CSAbilityTask::UK2Node_CSAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyActivateFunctionName = GET_FUNCTION_NAME_CHECKED(UGameplayTask, ReadyForActivation);
}

bool UK2Node_CSAbilityTask::IsCompatibleWithGraph(UEdGraph const* TargetGraph) const
{
	bool bIsCompatible = false;
	EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
	bool const bAllowLatentFuncs = (GraphType == GT_Ubergraph || GraphType == GT_Macro);
	
	if (bAllowLatentFuncs)
	{
		UBlueprint* MyBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (MyBlueprint && MyBlueprint->GeneratedClass)
		{
			if (MyBlueprint->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
			{
				bIsCompatible = true;
			}
		}
	}
	return bIsCompatible;
}

FText UK2Node_CSAbilityTask::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(FName::NameToDisplayString(ProxyFactoryFunctionName.ToString(), false));
}

void UK2Node_CSAbilityTask::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	static const FName RequiresConnectionMeta(TEXT("RequiresConnection"));
	for (TFieldIterator<FProperty> PropertyIt(ProxyClass); PropertyIt; ++PropertyIt)
	{
		if (FMulticastDelegateProperty* Property = CastField<FMulticastDelegateProperty>(*PropertyIt))
		{
			if (Property->GetBoolMetaData(RequiresConnectionMeta))
			{
				if (UEdGraphPin* DelegateExecPin = FindPin(Property->GetFName()))
				{
					if (DelegateExecPin->LinkedTo.Num() < 1)
					{
						const FText MessageText = FText::Format(LOCTEXT("NoConnectionToRequiredExecPin", "@@ - Unhandled event.  You need something connected to the '{0}' pin"), FText::FromName(Property->GetFName()));
						MessageLog.Warning(*MessageText.ToString(), this);
					}
				}
			}
		}
	}
}

void UK2Node_CSAbilityTask::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* TargetClass = UAbilityTask::StaticClass();

	for (TObjectIterator<UCSClass> ClassIt; ClassIt; ++ClassIt)
	{
		UCSClass* ManagedClass = *ClassIt;

		if (ManagedClass->HasAnyClassFlags(CLASS_Abstract) || !ManagedClass->IsChildOf(TargetClass) || FCSClassUtilities::IsSkeletonType(ManagedClass))
		{
			continue;
		}

		for (TFieldIterator<UFunction> FuncIt(ManagedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;

			if (!Function->HasAnyFunctionFlags(FUNC_Static))
			{
				continue;
			}

			FObjectProperty* ReturnProperty = CastField<FObjectProperty>(Function->GetReturnProperty());
			if (!ReturnProperty || !ReturnProperty->PropertyClass || !ReturnProperty->PropertyClass->IsChildOf(TargetClass))
			{
				continue;
			}

			// Skip generic factory functions (e.g. K2_CreateAbilityTask) that exist on base classes
			// and return their own type. These have DeterminesOutputType metadata.
			if (Function->HasMetaData(TEXT("DeterminesOutputType")))
			{
				continue;
			}

			UBlueprintNodeSpawner* Spawner = UBlueprintFunctionNodeSpawner::Create(Function);
			Spawner->NodeClass = GetClass();

			TWeakObjectPtr<UFunction> FunctionPtr = MakeWeakObjectPtr(const_cast<UFunction*>(Function));
			Spawner->CustomizeNodeDelegate.BindLambda([FunctionPtr](UEdGraphNode* NewNode, bool)
			{
				UK2Node_CSAbilityTask* TaskNode = CastChecked<UK2Node_CSAbilityTask>(NewNode);
				if (FunctionPtr.IsValid())
				{
					UFunction* Func = FunctionPtr.Get();
					FObjectProperty* RetProp = CastFieldChecked<FObjectProperty>(Func->GetReturnProperty());
					TaskNode->ProxyFactoryFunctionName = Func->GetFName();
					TaskNode->ProxyFactoryClass = Func->GetOuterUClass();
					TaskNode->ProxyClass = RetProp->PropertyClass;
				}
			});

			ActionRegistrar.AddBlueprintAction(ManagedClass, Spawner);
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
