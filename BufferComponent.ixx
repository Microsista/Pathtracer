module;
#include "RayTracingHlslCompat.hlsli"
export module BufferComponent;

import ConstantBuffer;
import DeviceResources;
import StructuredBuffer;

export class BufferComponent {
    ConstantBuffer<SceneConstantBuffer>* sceneCB;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>* filterCB;
    //StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer;
    DeviceResources* deviceResources;
    UINT NUM_BLAS;

public:
    BufferComponent(
        ConstantBuffer<SceneConstantBuffer>* sceneCB,
        ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>* filterCB,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer,
        DeviceResources* deviceResources,
        UINT NUM_BLAS)
        :
        sceneCB{ sceneCB },
        filterCB{ filterCB },
        //trianglePrimitiveAttributeBuffer{ trianglePrimitiveAttributeBuffer },
        deviceResources{ deviceResources },
        NUM_BLAS{ NUM_BLAS }
    {}

    void CreateConstantBuffers() {
        auto device = deviceResources->GetD3DDevice();
        auto frameCount = deviceResources->GetBackBufferCount();

        sceneCB->Create(device, frameCount, L"Scene Constant Buffer");
        filterCB->Create(device, frameCount, L"Filter Constant Buffer");
    }

    void CreateTrianglePrimitiveAttributesBuffers() {
        auto device = deviceResources->GetD3DDevice();
        auto frameCount = deviceResources->GetBackBufferCount();

        //(*trianglePrimitiveAttributeBuffer).Create(device, NUM_BLAS, frameCount, L"Triangle primitive attributes");
    }
};