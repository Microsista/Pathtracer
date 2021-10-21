module;
#include "d3dx12.h"
#include "RayTracingHlslCompat.hlsli"
export module PsoComponent;

import ShaderComponent;
import ShaderTableComponent;
import RootSignatureComponent;
import DirectXRaytracingHelper;
import DXSampleHelper;

export class PsoComponent {
    UINT MAX_RAY_RECURSION_DEPTH;
    ID3D12RootSignature* raytracingGlobalRootSignature;

public:
    PsoComponent() {}

    void CreateRaytracingPipelineStateObject() {
        CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

        CreateDxilLibrarySubobjects(&raytracingPipeline);
        CreateHitGroupSubobjects(&raytracingPipeline);

        auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();

        UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));

        UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);

        shaderConfig->Config(payloadSize, attributeSize);

        CreateLocalRootSignatureSubobjects(&raytracingPipeline);

        auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(raytracingGlobalRootSignature);

        auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

        UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
        pipelineConfig->Config(maxRecursionDepth);

        PrintStateObjectDesc(raytracingPipeline);

        ThrowIfFailed(dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
};