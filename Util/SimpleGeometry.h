//#pragma once
//
//#include "Util/DeviceResources.h"
//#include "Core/DirectXRaytracingHelper.h"
//#include "Core/stdafx.h"
//
//void BuildCubeGeometry(ID3D12Device)
//{
//	auto device = m_device
//}

 //auto device = m_deviceResources->GetD3DDevice();

    //// Build cube geometry
    //Index localIndices[] = {
    //    // front wall
    //    0, 3, 1,
    //    0, 2, 3,

    //    // back wall
    //    0 + 4, 1 + 4, 3 + 4,
    //    3 + 4, 0 + 4, 2 + 4,

    //    // left wall
    //    0 + 8, 3 + 8, 1 + 8,
    //    0 + 8, 2 + 8, 3 + 8,

    //    // right wall
    //    1 + 12, 3 + 12, 0 + 12,
    //    3 + 12, 2 + 12, 0 + 12,

    //    // bottom wall
    //    0 + 16, 1 + 16, 2 + 16,
    //    0 + 16, 2 + 16, 3 + 16,

    //    // top wall
    //    0 + 16, 2 + 16, 1 + 16,
    //    0 + 16, 3 + 16, 2 + 16,

    //};

    //Vertex localVertices[] = {
    //        // front wall
    //        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
    //        { XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
    //        { XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

    //        // back wall
    //        { XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
    //        { XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
    //        { XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f) },

    //        // left wall
    //        { XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

    //        // right wall
    //        { XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },

    //        // bottom wall
    //        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
    //        { XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

    //        // bottom wall
    //        { XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
    //        { XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
    //        { XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f) }
    //    };

    /*indices.insert(indices.end(), localIndices, localIndices + sizeof(localIndices));
    vertices.insert(vertices.end(), localVertices, localVertices + sizeof(localVertices));*/