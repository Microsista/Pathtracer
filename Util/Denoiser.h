#pragma once
#include "RTEffectsGpuKernels.h"

class Pathtracer;

class Denoiser {
public:
	enum DenoiseStage {
		Denoise_Stage1_TemporalSupersamplingReverseReproject = 0x1 << 0,
		Denoise_Stage2_Denoise = 0x1 << 1,
		Denoise_StageAll = Denoise_Stage1_TemporalSupersamplingReverseReproject | Denoise_Stage2_Denoise
	};

	// Public methods
	void Setup(std::shared_ptr<DX::DeviceResources> deviceResources, std::shared_ptr<DX::DescriptorHeap> descriptorHeap);
	void Run(Pathtracer& pathtracer, DenoiseStage stage = Denoise_StageAll);

private:
	void TemporalSupersamplingReverseReproject(Pathtracer& pathtracer);

	void CreateDeviceDependentResources();
	void CreateAuxilaryDeviceResources();

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	std::shared_ptr<DX::DescriptorHeap> m_cbvSrvUavHeap;

	UINT m_temporalCacheCurrentFrameResourceIndex = 0;

	// RTEffectsGpuKernels
	RTEffectsGpuKernels::FillInCheckerboard      m_fillInCheckerboardKernel;
	RTEffectsGpuKernels::GaussianFilter          m_gaussianSmoothingKernel;
	RTEffectsGpuKernels::TemporalSupersampling_ReverseReproject m_temporalCacheReverseReprojectKernel;
	RTEffectsGpuKernels::TemporalSupersampling_BlendWithCurrentFrame m_temporalCacheBlendWithCurrentFrameKernel;
	RTEffectsGpuKernels::AtrousWaveletTransformCrossBilateralFilter m_atrousWaveletTransformFilter;
	RTEffectsGpuKernels::CalculateMeanVariance   m_calculateMeanVarianceKernel;
	RTEffectsGpuKernels::DisocclusionBilateralFilter m_disocclusionBlurKernel;

public:
	static const UINT c_MaxNumDisocclusionBlurPasses = 6;
};