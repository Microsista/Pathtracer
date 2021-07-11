#pragma once

#include "RayTracingHlslCompat.h"

namespace GlobalRootSignature {
    namespace Slot {
        enum Enum {
            OutputView = 0,
            AccelerationStructure,
            SceneConstant,
            TriangleAttributeBuffer,
            ReflectionBuffer,
            ShadowBuffer,
            NormalDepth,
            Count
        };
    }
}

namespace LocalRootSignature {
    namespace Type {
        enum Enum {
            Triangle = 0,
            Count
        };
    }
}

namespace LocalRootSignature {
    namespace Triangle {
        namespace Slot {
            enum Enum {
                MaterialConstant = 0,
                GeometryIndex,
                IndexBuffer,
                VertexBuffer,
                DiffuseTexture,
                Count
            };
        }
        struct RootArguments {
            PrimitiveConstantBuffer materialCb;
            PrimitiveInstanceConstantBuffer triangleCB;
            // Bind each resource via a descriptor.
        // This design was picked for simplicity, but one could optimize for shader record size by:
        //    1) Binding multiple descriptors via a range descriptor instead.
        //    2) Storing 4 Byte indices (instead of 8 Byte descriptors) to a global pool resources.
            D3D12_GPU_DESCRIPTOR_HANDLE indexBufferGPUHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE vertexBufferGPUHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE diffuseTextureGPUHandle;
        };
    }
}

namespace LocalRootSignature {
    inline UINT MaxRootArgumentsSize()
    {
        return sizeof(Triangle::RootArguments);
    }
}

namespace GeometryType {
    enum Enum {
        Triangle = 0,
        Coordinates,
        Skull,
        Table,
        Lamps,
        SunTemple,
        Count
    };
}

namespace GpuTimers {
    enum Enum {
        Raytracing = 0,
        Count
    };
}

// Bottom-level acceleration structures (BottomLevelASType).
// This sample uses two BottomLevelASType, one for AABB and one for Triangle geometry.
// Mixing of geometry types within a BLAS is not supported.
namespace BottomLevelASType = GeometryType;


namespace GBufferResource {
    enum Enum {
        HitPosition = 0,	// 3D position of hit.
        SurfaceNormalDepth,	// Encoded normal and linear depth.
        Depth,          // Linear depth of the hit.
        PartialDepthDerivatives,
        MotionVector,
        ReprojectedNormalDepth,
        Color,
        AOSurfaceAlbedo,
        Count
    };
}