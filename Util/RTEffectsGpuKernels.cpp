#include "../Core/stdafx.h"
#include "RTEffectsGpuKernels.h"

void RTEffectsGpuKernels::FillInCheckerboard::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::FillInCheckerboard::Run(ID3D12GraphicsCommandList4* commandList, ID3D12DescriptorHeap* descriptorHeap, UINT width, UINT height, D3D12_GPU_DESCRIPTOR_HANDLE inputOutputResourceHandle, bool fillEvenPixels)
{
}

void RTEffectsGpuKernels::GaussianFilter::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::GaussianFilter::Run(ID3D12GraphicsCommandList4* commandList, UINT width, UINT height, FilterType type, ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputResourceHandle)
{
}

void RTEffectsGpuKernels::TemporalSupersampling_ReverseReproject::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::TemporalSupersampling_ReverseReproject::Run(ID3D12GraphicsCommandList4* commandList, UINT width, UINT height, ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputCurrentFrameNormalDepthResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCurrentFrameLinearDepthDerivativeResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputReprojectedNormalDepthResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputTextureSpaceMotionVectorResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCachedValueResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCachedNormalDepthResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCachedTsppResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCachedSquaredMeanValue, D3D12_GPU_DESCRIPTOR_HANDLE inputCachedRayHitDistanceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputReprojectedCacheTsppResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputReprojectedCacheValuesResourceHandle, bool usingBilateralDownsampledBuffers, float depthSigma)
{
}

void RTEffectsGpuKernels::TemporalSupersampling_BlendWithCurrentFrame::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::TemporalSupersampling_BlendWithCurrentFrame::Run(ID3D12GraphicsCommandList4* commandList, UINT width, UINT height, ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputCurrentFrameValueResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCurrentFrameLocalMeanVarianceResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputCurrentFrameRayHitDistanceResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputOutputValueResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputOutputTsppResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputOutputSquaredMeanValueResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputOutputRayHitDistanceResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputReprojectedCacheValuesResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputVarianceResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputBlurStrengthResourceHandle, float minSmoothingFactor, bool forceUseMinSmoothingFactor, bool clampCachedValues, float clampStdDevGamma, float clampMinStdDevTolerance, UINT minTsppToUseTemporalVariance, UINT lowTsppBlurStrengthMaxTspp, float lowTsppBlurStrengthDecayConstant, bool doCheckerboardSampling, bool checkerboardLoadEvenPixels, float clampDifferenceToTsppScale)
{
}

void RTEffectsGpuKernels::AtrousWaveletTransformCrossBilateralFilter::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::AtrousWaveletTransformCrossBilateralFilter::Run(ID3D12GraphicsCommandList4* commandList, ID3D12DescriptorHeap* descriptorHeap, FilterType type, D3D12_GPU_DESCRIPTOR_HANDLE inputValuesResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputNormalsResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputVarianceResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputHitDistanceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputPartialDistanceDerivativesResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputTsppResourceHandle, GpuResource* outputResource, float valueSigma, float depthSigma, float normalSigma, bool perspectiveCorrectDepthInterpolation, bool useAdaptiveKernelSize, float kernelRadiusLerfCoef, float rayHitDistanceToKernelWidthScale, float rayHitDistanceToKernelSizeScaleExponent, UINT minKernelWidth, UINT maxKernelWidth, bool usingBilateralDownsampledBuffers, float minVarianceToDenoise, float depthWeightCutoff)
{
}

void RTEffectsGpuKernels::CalculateMeanVariance::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::CalculateMeanVariance::Run(ID3D12GraphicsCommandList4* commandList, ID3D12DescriptorHeap* descriptorHeap, UINT width, UINT height, D3D12_GPU_DESCRIPTOR_HANDLE inputValuesResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE outputMeanVarianceResourceHandle, UINT kernelWidth, bool doCheckerboardSampling, bool checkerboardLoadEvenPixels)
{
}

void RTEffectsGpuKernels::DisocclusionBilateralFilter::Initialize(ID3D12Device5* device, UINT frameCount, UINT numCallsPerFrame)
{
}

void RTEffectsGpuKernels::DisocclusionBilateralFilter::Run(ID3D12GraphicsCommandList4* commandList, UINT filterStep, ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputDepthResourceHandle, D3D12_GPU_DESCRIPTOR_HANDLE inputBlurStrengthResourceHandle, GpuResource* inputOutputResource)
{
}
