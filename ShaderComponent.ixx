module;
#include "d3dx12.h"

#include "Obj/Debug/CompiledShaders/ViewRG.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceCH.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/ShadowMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/CompositionCS.hlsl.h"
#include "Obj/Debug/CompiledShaders/BlurCS.hlsl.h"
export module ShaderComponent;

export class ShaderComponent {
public:
    ShaderComponent() {}

    void CreateDxilLibrarySubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
        const unsigned char* compiledShaderByteCode[] = { g_pViewRG, g_pRadianceCH, g_pRadianceMS, g_pShadowMS };
        const unsigned int compiledShaderByteCodeSizes[] = { ARRAYSIZE(g_pViewRG), ARRAYSIZE(g_pRadianceCH), ARRAYSIZE(g_pRadianceMS), ARRAYSIZE(g_pShadowMS) };
        auto size = sizeof(compiledShaderByteCode) / sizeof(compiledShaderByteCode[0]);
        for (int i = 0; i < size; i++) {
            auto lib = raytracingPipeline->CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            auto libDXIL = CD3DX12_SHADER_BYTECODE((void*)compiledShaderByteCode[i], compiledShaderByteCodeSizes[i]);
            lib->SetDXILLibrary(&libDXIL);
        };

        // Use default shader exports for a DXIL library/collection subobject ~ surface all shaders.
    }
};