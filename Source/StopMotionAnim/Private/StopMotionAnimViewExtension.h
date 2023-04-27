// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "SceneViewExtension.h"

class AStopMotionAnimActor;

/**
 *	
 */
class FStopMotionAnimViewExtension : public FSceneViewExtensionBase
{
public:
	FStopMotionAnimViewExtension(const FAutoRegister& AutoRegister, AStopMotionAnimActor* InOwner);

	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	//~ ISceneViewExtension interface

private:
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

	TWeakObjectPtr<AStopMotionAnimActor> Owner;
};