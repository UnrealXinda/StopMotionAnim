// Fill out your copyright notice in the Description page of Project Settings.

#include "StopMotionAnim.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FStopMotionAnimModule"

void FStopMotionAnimModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StopMotionAnim"));
	check(Plugin.IsValid());

	const FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/StopMotionAnim"), PluginShaderDir);
}

void FStopMotionAnimModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FStopMotionAnimModule, StopMotionAnim)