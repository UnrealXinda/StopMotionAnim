// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StopMotionAnimActor.generated.h"

class FStopMotionAnimViewExtension;

UCLASS()
class STOPMOTIONANIM_API AStopMotionAnimActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AStopMotionAnimActor();

	virtual void Tick(float DeltaTime) override;
	bool ShouldEnableViewExtension() const;
	bool ShouldSaveFrame() const;

	UPROPERTY(VisibleInstanceOnly, Transient, DuplicateTransient, Category = "Stop Motion Anim")
	UTextureRenderTarget2D* LastSavedFrame;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Stop Motion Anim", meta = (ClampMin = 1, ClampMax = 120))
	uint32 FramesPerSecond;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Stop Motion Anim (Debug)")
	float SecondsUntilNextFrameSave;

	TSharedPtr<FStopMotionAnimViewExtension, ESPMode::ThreadSafe> ViewExtension;
};