#include "BentoFlipbookComponent.h"
#include "PaperFlipbook.h"

UBentoFlipbookComponent::UBentoFlipbookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBentoFlipbookComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	int32 CurrentFrame = GetPlaybackPositionInFrames();
	if (CurrentFrame != LastObservedFrameIndex)
	{
		LastObservedFrameIndex = CurrentFrame;
		OnFrameChanged.Broadcast(CurrentFrame);

		if (!IsLooping() && GetFlipbook() != nullptr)
		{
			if (CurrentFrame >= GetFlipbook()->GetNumFrames() - 1)
			{
				OnAnimationFinished.Broadcast(GetFlipbook()->GetName());
			}
		}
	}
}
