module;
#include <DirectXMath.h>
export module Directions;

export namespace Directions {
    auto FORWARD = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f };
    auto BACKWARD = DirectX::XMVECTOR{ 0.0f, 0.0f, -1.0f };
    auto LEFT = DirectX::XMVECTOR{ -1.0f, 0.0f, 0.0f };
    auto RIGHT = DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f };
    auto UP = DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f };
    auto DOWN = DirectX::XMVECTOR{ 0.0f, -1.0f, 0.0f };
}