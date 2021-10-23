#pragma once

#include "RayTracingHlslCompat.hlsli"

export {
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
                MotionVector,
                PrevHitPosition,
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
                    NormalTexture,
                    SpecularTexture,
                    EmissiveTexture,
                    Count
                };
            }
            struct RootArguments {
                PrimitiveConstantBuffer materialCb;
                PrimitiveInstanceConstantBuffer triangleCB;

                D3D12_GPU_DESCRIPTOR_HANDLE indexBufferGPUHandle;
                D3D12_GPU_DESCRIPTOR_HANDLE vertexBufferGPUHandle;
                D3D12_GPU_DESCRIPTOR_HANDLE diffuseTextureGPUHandle;
                D3D12_GPU_DESCRIPTOR_HANDLE normalTextureGPUHandle;
                D3D12_GPU_DESCRIPTOR_HANDLE specularTextureGPUHandle;
                D3D12_GPU_DESCRIPTOR_HANDLE emittanceTextureGPUHandle;
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

    namespace BottomLevelASType = GeometryType;

    namespace GBufferResource {
        enum Enum {
            HitPosition = 0,
            SurfaceNormalDepth,
            Depth,
            PartialDepthDerivatives,
            MotionVector,
            ReprojectedNormalDepth,
            Color,
            AOSurfaceAlbedo,
            Count
        };
    }
}