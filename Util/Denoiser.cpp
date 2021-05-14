#include "../Core/stdafx.h"
#include "Denoiser.h"
#include "Composition.h"
#include "../Core/AdvancedRoom.h"


void Denoiser::Setup(std::shared_ptr<DX::DeviceResources> deviceResources, std::shared_ptr<DX::DescriptorHeap> descriptorHeap)
{
	m_deviceResources = deviceResources;
	m_cbvSrvUavHeap = descriptorHeap;

	CreateDeviceDependentResources();
}

// Run() can be optionally called in two explicit stages. This can
// be beneficial to retrieve temporally reprojected values 
// and configure current frame AO raytracing off of that 
// (such as vary spp based on average ray hit distance or tspp).
// Otherwise, all denoiser steps can be run via a single execute call.
void Denoiser::Run(Pathtracer& pathtracer, DenoiseStage stage)
{
	auto commandList = m_deviceResources->GetCommandList();


	if (stage & Denoise_Stage1_TemporalSupersamplingReverseReproject)
	{
		TemporalSupersamplingReverseReproject(pathtracer);
	}

}

// Retrieves values from previous frame via reverse reprojection.
void Denoiser::TemporalSupersamplingReverseReproject(Pathtracer& pathtracer)
{
	auto commandList = m_deviceResources->GetCommandList();
	//auto resourceStateTracker = m_deviceResources->GetGpuResourceStateTracker();

	// Ping-pong input output indices across frames.
	UINT temporalCachePreviousFrameResourceIndex = m_temporalCacheCurrentFrameResourceIndex;
	m_temporalCacheCurrentFrameResourceIndex = (m_temporalCacheCurrentFrameResourceIndex + 1) % 2;
}

// Create resources that depend on the device.
void Denoiser::CreateDeviceDependentResources()
{
	CreateAuxilaryDeviceResources();
}

void Denoiser::CreateAuxilaryDeviceResources()
{
	auto device = m_deviceResources->GetD3DDevice();
	m_fillInCheckerboardKernel.Initialize(device, App::FrameCount);
	m_gaussianSmoothingKernel.Initialize(device, App::FrameCount);
	m_temporalCacheReverseReprojectKernel.Initialize(device, App::FrameCount);
	m_temporalCacheBlendWithCurrentFrameKernel.Initialize(device, App::FrameCount);
	m_atrousWaveletTransformFilter.Initialize(device, App::FrameCount);
	m_calculateMeanVarianceKernel.Initialize(device, App::FrameCount);
	m_disocclusionBlurKernel.Initialize(device, App::FrameCount, c_MaxNumDisocclusionBlurPasses);
}
