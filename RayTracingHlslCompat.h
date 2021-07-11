#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
typedef UINT Index;
#else
using namespace DirectX;

// Shader will use byte encoding to access vertex indices.
typedef UINT Index;
#endif

// PERFORMANCE TIP: Set max recursion depth as low as needed
// as drivers may apply optimization strategies for low recursion depths.
#define MAX_RAY_RECURSION_DEPTH 3    // ~ primary rays + reflections + shadow rays from reflected geometry.
#define SAMPLER_FILTER D3D12_FILTER_ANISOTROPIC

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
    UINT DepthNumMantissaBits;      // Number of Mantissa Bits in the floating format of the input depth resources format.
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
    float    elapsedTime;                 // Elapsed application time.
};

// Attributes per primitive type.
struct PrimitiveConstantBuffer
{
    XMFLOAT4 albedo;
    float reflectanceCoef;
    float diffuseCoef;
    float metalness;
    float roughness;
    float stepScale;                      // Step scale for ray marching of signed distance primitives. 
                                          // - Some object transformations don't preserve the distances and 
                                          //   thus require shorter steps.
    float shaded;
    XMFLOAT2 padding;
};

struct PrimitiveMaterialBuffer
{
    XMFLOAT3 Kd; // diffuse coefficient
    XMFLOAT3 Ks; // specular coefficient
    XMFLOAT3 Kr; // reflectance coefficient
    XMFLOAT3 Kt; // transparency coefficient
    XMFLOAT3 opacity; // opacity
    XMFLOAT3 eta; // n1/n2
    float roughness; // roughness
    BOOL hasDiffuseTexture;
    BOOL hasNormalTexture;
    BOOL hasPerVertexTangents;
    //MaterialType::Type type;
    float padding;
};

namespace MaterialType {
    enum Type {
        Default,
        Matte,      // Lambertian scattering
        Mirror,     // Specular reflector that isn't modified by the Fresnel equations.
        AnalyticalCheckerboardTexture
    };
}

// Attributes per primitive instance.
struct PrimitiveInstanceConstantBuffer
{
    UINT instanceIndex;  
};

// Dynamic attributes per primitive instance.
struct PrimitiveInstancePerFrameBuffer
{
    XMMATRIX localSpaceToBottomLevelAS;   // Matrix from local primitive space to bottom-level object space.
    XMMATRIX bottomLevelASToLocalSpace;   // Matrix from bottom-level object space to local primitive space.
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


// Ray types traced in this sample.
namespace RayType {
    enum Enum {
        Radiance = 0,   // ~ Primary, reflected camera/view rays calculating color for each hit.
        Shadow,         // ~ Shadow/visibility rays, only testing for occlusion
        Count
    };
}

namespace TraceRayParameters
{
    static const UINT InstanceMask = 0xffff;   // Everything is visible.
    namespace HitGroup {
        static const UINT Offset[RayType::Count] =
        {
            0, // Radiance ray
            1  // Shadow ray
        };
        static const UINT GeometryStride = RayType::Count;
    }
    namespace MissShader {
        static const UINT Offset[RayType::Count] =
        {
            0, // Radiance ray
            1  // Shadow ray
        };
    }
}

// From: http://blog.selfshadow.com/publications/s2015-shading-course/hoffman/s2015_pbs_physics_math_slides.pdf
static const XMFLOAT4 ChromiumReflectance = XMFLOAT4(0.549f, 0.556f, 0.554f, 1.0f);

static const XMFLOAT4 BackgroundColor = XMFLOAT4(0.8f, 0.9f, 1.0f, 1.0f);
static const float InShadowRadiance = 0.35f;

namespace TriangleGeometry {
    enum Enum {
        Room = 0,
        Count
    };
}

namespace CoordinateGeometry {
    enum Enum {
        Coordinates = 0,
        Count
    };
}


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

namespace HouseGeometry {
    enum Enum {
        HouseMesh1 = 0,
        HouseMesh2,
        HouseMesh3,
        HouseMesh4,
        HouseMesh5,
        HouseMesh6,
        HouseMesh7,
        HouseMesh8,
        HouseMesh9,
        HouseMesh10,
        HouseMesh11,
        HouseMesh12,
        HouseMesh13,
        HouseMesh14,
        HouseMesh15,
        HouseMesh16,
        HouseMesh17,
        HouseMesh18,
        HouseMesh19,
        HouseMesh20,
        HouseMesh21,
        HouseMesh22,
        HouseMesh23,
        HouseMesh24,
        HouseMesh25,
        HouseMesh26,
        HouseMesh27,

        Count
    };
}

namespace AllGeometry {
    enum Enum {
        Room = 0,
        Coordinates,
        Skull,
        ModelMesh1,
        ModelMesh2,
        ModelMesh3,
        ModelMesh4,
        LampMesh1,
        LampMesh2,
        LampMesh3,
        LampMesh4,
        HouseMesh1,
        HouseMesh2,
        HouseMesh3,
        HouseMesh4,
        HouseMesh5,
        HouseMesh6,
        HouseMesh7,
        HouseMesh8,
        HouseMesh9,
        HouseMesh10,
        HouseMesh11,
        HouseMesh12,
        HouseMesh13,
        HouseMesh14,
        HouseMesh15,
        HouseMesh16,
        HouseMesh17,
        HouseMesh18,
        HouseMesh19,
        HouseMesh20,
        HouseMesh21,
        HouseMesh22,
        HouseMesh23,
        HouseMesh24,
        HouseMesh25,
        HouseMesh26,
        HouseMesh27,
    
        Count
    };

}


#endif // RAYTRACINGHLSLCOMPAT_H