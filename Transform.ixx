module;
#include <DirectXMath.h>
export module Transform;

using namespace DirectX;

export struct Transform {
    XMFLOAT3 translation;
    XMFLOAT3 scale;
    float rotation;
};