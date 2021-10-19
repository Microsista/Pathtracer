#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
typedef uint Index;
typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef float4x4 XMMATRIX;
typedef uint UINT;
typedef uint2 XMUINT2;
typedef uint3 XMUINT3;
typedef int2 XMINT2;
typedef int BOOL;
#else
#include <DirectXMath.h>
#include <d3d12.h>
#include <Windows.h>
typedef UINT Index;
using namespace DirectX;
#endif

#define MAX_RAY_RECURSION_DEPTH 3
#define SAMPLER_FILTER D3D12_FILTER_ANISOTROPIC

struct GeometryBuffer {
    XMFLOAT3 _virtualHitPosition;
};

struct AtrousWaveletTransformFilterConstantBuffer
{
    XMUINT2 textureDim;
    float depthWeightCutoff;
    bool usingBilateralDownsampledBuffers;

    BOOL useAdaptiveKernelSize;
    float kernelRadiusLerfCoef;
    UINT minKernelWidth;
    UINT maxKernelWidth;

    float rayHitDistanceToKernelWidthScale;
    float rayHitDistanceToKernelSizeScaleExponent;
    BOOL perspectiveCorrectDepthInterpolation;
    float minVarianceToDenoise;

    float valueSigma;
    float depthSigma;
    float normalSigma;
    UINT DepthNumMantissaBits;
};

struct FilterConstantBuffer
{
    XMUINT2 textureDim;
    UINT step;
    float padding;
};

struct CalculateMeanVarianceConstantBuffer
{
    XMUINT2 textureDim;
    UINT kernelWidth;
    UINT kernelRadius;

    BOOL doCheckerboardSampling;
    BOOL areEvenPixelsActive;
    UINT pixelStepY;
    float padding;
};

struct TemporalSupersampling_ReverseReprojectConstantBuffer
{
    XMUINT2 textureDim;
    XMFLOAT2 invTextureDim;

    float depthSigma;
    UINT DepthNumMantissaBits;
    BOOL usingBilateralDownsampledBuffers;
    float padding;
};

struct TemporalSupersampling_BlendWithCurrentFrameConstantBuffer
{
    float stdDevGamma;
    BOOL clampCachedValues;
    float clamping_minStdDevTolerance;
    float padding;

    float clampDifferenceToTsppScale;
    BOOL forceUseMinSmoothingFactor;
    float minSmoothingFactor;
    UINT minTsppToUseTemporalVariance;

    UINT blurStrength_MaxTspp;
    float blurDecayStrength;
    BOOL checkerboard_enabled;
    BOOL checkerboard_areEvenPixelsActive;
};

struct TextureDimConstantBuffer
{
    XMUINT2 textureDim;
    XMFLOAT2 invTextureDim;
};


struct ProceduralPrimitiveAttributes
{
    XMFLOAT3 normal;
};

struct RayPayload
{
    XMFLOAT4 color;
    UINT   recursionDepth;
    float inShadow;
    float depth;
    XMFLOAT3 prevHitPosition;
    XMFLOAT3 hitPosition;
};

struct ShadowRayPayload
{
    bool hit;
};

struct SceneConstantBuffer
{
    XMMATRIX projectionToWorld;
    XMVECTOR cameraPosition;
    XMVECTOR lightPosition;
    XMVECTOR lightAmbientColor;
    XMVECTOR lightDiffuseColor;
    float    reflectance;
    float    elapsedTime;
    int frameIndex;

    XMFLOAT3 prevFrameCameraPosition;
    XMMATRIX prevFrameProjToViewCameraAtOrigin;
    XMMATRIX prevFrameViewProj;
};

struct PrimitiveConstantBuffer
{
    XMFLOAT4 albedo;
    float reflectanceCoef;
    float diffuseCoef;
    float metalness;
    float roughness;
    float stepScale;
    float shaded;
    XMFLOAT2 padding;
};

struct PrimitiveMaterialBuffer
{
    XMFLOAT3 Kd; 
    XMFLOAT3 Ks; 
    XMFLOAT3 Kr;
    XMFLOAT3 Kt;
    XMFLOAT3 Ke;
    XMFLOAT3 opacity; 
    XMFLOAT3 eta;
    float roughness; 
    BOOL hasDiffuseTexture;
    BOOL hasNormalTexture;
    BOOL hasPerVertexTangents;
    float padding;
};

namespace MaterialType {
    enum Type {
        Default,
        Matte,    
        Mirror,    
        AnalyticalCheckerboardTexture
    };
}

struct PrimitiveInstanceConstantBuffer
{
    UINT instanceIndex;
};

struct PrimitiveInstancePerFrameBuffer
{
    XMMATRIX localSpaceToBottomLevelAS;   
    XMMATRIX bottomLevelASToLocalSpace;  
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};

struct VertexPositionNormalTextureTangent
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 textureCoordinate;
    XMFLOAT3 tangent;
};

namespace RayType {
    enum Enum {
        Radiance = 0,
        Shadow,       
        Count
    };
}

namespace TraceRayParameters
{
    static const UINT InstanceMask = 0xffff;   
    namespace HitGroup {
        static const UINT Offset[RayType::Count] =
        {
            0,
            1 
        };
        static const UINT GeometryStride = RayType::Count;
    }
    namespace MissShader {
        static const UINT Offset[RayType::Count] =
        {
            0, 
            1 
        };
    }
}

static const XMFLOAT4 ChromiumReflectance = XMFLOAT4(0.549f, 0.556f, 0.554f, 1.0f);
static const XMFLOAT4 BackgroundColor = XMFLOAT4(0.8f, 0.9f, 1.0f, 1.0f);
static const float InShadowRadiance = 0.35f;

namespace SkullGeometry {
    enum Enum {
        Skull = 0,
        Count
    };
}

namespace TableGeometry {
    enum Enum {
        ModelMesh1 = 0,
        ModelMesh2,
        ModelMesh3,
        ModelMesh4,
        Count
    };
}

namespace LampsGeometry {
    enum Enum {
        LampMesh1 = 0,
        LampMesh2,
        LampMesh3,
        LampMesh4,
        Count
    };
}

#endif