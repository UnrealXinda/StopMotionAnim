// Fill out your copyright notice in the Description page of Project Settings.


#include "StopMotionAnimActor.h"
#include "StopMotionAnimViewExtension.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

// Sets default values
AStopMotionAnimActor::AStopMotionAnimActor()
{
	PrimaryActorTick.bCanEverTick = true;
	FramesPerSecond = 8;
}

// Called when the game starts or when spawned
void AStopMotionAnimActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(LastSavedFrame))
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameViewportClient* Client = World->GetGameViewport())
			{
				const FIntPoint ViewSize = Client->Viewport->GetSizeXY();

				LastSavedFrame = UKismetRenderingLibrary::CreateRenderTarget2D(this, ViewSize.X, ViewSize.Y, RTF_RGBA16f);
			}
		}
	}

	ViewExtension = FSceneViewExtensions::NewExtension<FStopMotionAnimViewExtension>(this);
	SecondsUntilNextFrameSave = 0.f;
}

// Called every frame
void AStopMotionAnimActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float RenderInterval = 1.0f / FramesPerSecond;
	const float TimeDilation = GetActorTimeDilation();
	const float RealDeltaTime = DeltaTime / TimeDilation;

	while (SecondsUntilNextFrameSave < 0)
	{
		SecondsUntilNextFrameSave += RenderInterval;
	}

	SecondsUntilNextFrameSave -= RealDeltaTime;
}

bool AStopMotionAnimActor::ShouldEnableViewExtension() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->IsGameWorld();
	}

	return false;
}

bool AStopMotionAnimActor::ShouldSaveFrame() const
{
	return SecondsUntilNextFrameSave < 0;
}