#pragma once

#include "CoreMinimal.h"
#include "PaperFlipbookComponent.h"
#include "BentoFlipbookComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBentoFrameChangedSignature, int32, CurrentFrame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBentoAnimationFinishedSignature, const FString&, AnimationName);

/**
 * Enhanced Paper2D Flipbook Component with M8 tight render geometry support
 * and dynamic frame notifications.
 */
UCLASS(ClassGroup = (Paper2D), meta = (BlueprintSpawnableComponent))
class BENTOPACK_API UBentoFlipbookComponent : public UPaperFlipbookComponent
{
	GENERATED_BODY()

public:
	UBentoFlipbookComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Emitted whenever the active flipbook frame changes */
	UPROPERTY(BlueprintAssignable, Category = "BentoPack")
	FBentoFrameChangedSignature OnFrameChanged;

	/** Emitted when a non-looping flipbook completes */
	UPROPERTY(BlueprintAssignable, Category = "BentoPack")
	FBentoAnimationFinishedSignature OnAnimationFinished;

	/** Path to the source .bento archive if linked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BentoPack")
	FString SourceBentoPath;

protected:
	int32 LastObservedFrameIndex = -1;
};
