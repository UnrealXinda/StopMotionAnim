// Fill out your copyright notice in the Description page of Project Settings.


#include "StopMotionAnimViewExtension.h"
#include "StopMotionAnimActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PostProcess/PostProcessMaterial.h"
#include "Renderer/Private/ScreenPass.h"

class FCopyTexturePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopyTexturePS);
	SHADER_USE_PARAMETER_STRUCT(FCopyTexturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCopyTexturePS, "/Plugin/StopMotionAnim/CopySceneColor.usf", "Main", SF_Pixel);

namespace
{
	FRHITexture* GetRHITexture(const UTextureRenderTarget2D* InTargetTexture)
	{
		FRHITexture* RHITexture = nullptr;

		if (IsValid(InTargetTexture))
		{
			const FTextureReferenceRHIRef TargetTextureRHI = InTargetTexture->TextureReference.TextureReferenceRHI;
			if (TargetTextureRHI.IsValid())
			{
				if (const FRHITextureReference* TextureReference = TargetTextureRHI->GetTextureReference())
				{
					RHITexture = TextureReference->GetReferencedTexture();
				}
			}
		}

		return RHITexture;
	}

	FScreenPassTexture ReturnUntouchedSceneColorForPostProcessing(const FPostProcessMaterialInputs& Inputs)
	{
		if (Inputs.OverrideOutput.IsValid())
		{
			return Inputs.OverrideOutput;
		}

		/** We don't want to modify scene texture in any way. We just want it to be passed back onto the next stage. */
		FScreenPassTexture SceneTexture = const_cast<FScreenPassTexture&>(Inputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
		return SceneTexture;
	}
}

FStopMotionAnimViewExtension::FStopMotionAnimViewExtension(const FAutoRegister& AutoRegister, AStopMotionAnimActor* InOwner) :
	FSceneViewExtensionBase{AutoRegister},
	Owner{InOwner}
{
}

void FStopMotionAnimViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	if (!IsValid(InView.ViewActor))
	{
		return;
	}

	const UWorld* World = InView.ViewActor->GetWorld();
	if (!(IsValid(World) && World->IsGameWorld()))
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = Owner->LastSavedFrame;
	if (!IsValid(RenderTarget))
	{
		return;
	}

	const FViewInfo& ViewInfo = StaticCast<const FViewInfo&>(InView);
	const FIntRect PrimaryViewRect = ViewInfo.ViewRect;
	const int32 ViewWidth = PrimaryViewRect.Width();
	const int32 ViewHeight = PrimaryViewRect.Height();

	if (ViewWidth > 0 && ViewHeight > 0)
	{
		if (ViewWidth != RenderTarget->SizeX ||
			ViewHeight != RenderTarget->SizeY)
		{
			RenderTarget->InitAutoFormat(ViewWidth, ViewHeight);
			RenderTarget->UpdateResourceImmediate(true);
		}
	}
}

int32 FStopMotionAnimViewExtension::GetPriority() const
{
	// Comes at the last. Wait until everything has finished
	return INT32_MIN;
}

bool FStopMotionAnimViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return Owner.IsValid() && Owner->ShouldEnableViewExtension();
}

void FStopMotionAnimViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	// Why tonemap doesn't work?
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(
			FAfterPassCallbackDelegate::CreateRaw(
				this, &FStopMotionAnimViewExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

FScreenPassTexture FStopMotionAnimViewExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder,
	const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	GraphBuilder.RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

	FScreenPassTexture SceneColor = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());

	if (!Owner.IsValid())
	{
		return SceneColor;
	}

	// TODO: how to figure out which view to apply the extension?
	if (!IsValid(View.ViewActor))
	{
		return SceneColor;
	}

	const UWorld* World = View.ViewActor->GetWorld();
	if (!(IsValid(World) && World->IsGameWorld()))
	{
		return SceneColor;
	}

	if (View.PlayerIndex != 0)
	{
		return SceneColor;
	}

	const UTextureRenderTarget2D* RenderTarget = Owner->LastSavedFrame;
	FRHITexture* RenderTargetRHI = GetRHITexture(RenderTarget);

	if (!RenderTargetRHI)
	{
		return SceneColor;
	}

	const FViewInfo& ViewInfo = StaticCast<const FViewInfo&>(View);
	const FRDGTextureRef SceneTextureRDG = SceneColor.Texture;
	const FRDGTextureRef RenderTargetRDG = RegisterExternalTexture(GraphBuilder, RenderTargetRHI, TEXT("LastSavedFrame"));

	// Save the frame
	if (Owner->ShouldSaveFrame())
	{
		const FScreenPassTextureViewport InputViewport{SceneTextureRDG};
		const FScreenPassTextureViewport OutputViewport{RenderTargetRDG};

		FCopyTexturePS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyTexturePS::FParameters>();
		Parameters->InputTexture = SceneColor.Texture;
		Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
		Parameters->RenderTargets[0] = FRenderTargetBinding{RenderTargetRDG, ERenderTargetLoadAction::ELoad};

		const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const TShaderMapRef<FCopyTexturePS> CopyTexturePS{GlobalShaderMap};

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("StopMotionAnim_CopySceneColor"),
			ViewInfo,
			OutputViewport,
			InputViewport,
			CopyTexturePS,
			Parameters);

		GraphBuilder.RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		return ReturnUntouchedSceneColorForPostProcessing(Inputs);
	}

	// Copy saved frame to scene color
	FScreenPassRenderTarget OutputRenderTarget = Inputs.OverrideOutput;

	// If the override output is provided it means that this is the last pass in post processing.
	if (!OutputRenderTarget.IsValid())
	{
		OutputRenderTarget = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder,
			SceneColor,
			ViewInfo.GetOverwriteLoadAction(),
			TEXT("OutputRenderTarget"));
	}

	const FScreenPassTextureViewport InputViewport{RenderTargetRDG};
	const FScreenPassTextureViewport OutputViewport(OutputRenderTarget);

	FCopyTexturePS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyTexturePS::FParameters>();
	Parameters->InputTexture = RenderTargetRDG;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = OutputRenderTarget.GetRenderTargetBinding();

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const TShaderMapRef<FCopyTexturePS> CopyTexturePS{GlobalShaderMap};

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("StopMotionAnim_CopySavedFrame"),
		ViewInfo,
		OutputViewport,
		InputViewport,
		CopyTexturePS,
		Parameters);

	GraphBuilder.RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	return MoveTemp(OutputRenderTarget);
}