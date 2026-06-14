#pragma once

#include "CoreMinimal.h"
#include "K2Node_BaseAsyncTask.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_CSAbilityTask.generated.h"

UCLASS()
class UNREALSHARPGAS_API UK2Node_CSAbilityTask : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()
public:
	UK2Node_CSAbilityTask(const FObjectInitializer& ObjectInitializer);

	// UEdGraphNode interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface
};
