module;
#include <string>
#include <DirectXMath.h>
export module Material;

using namespace std;
using namespace DirectX;

export struct Material {
    Material() {}

    unsigned int id;

    string newmtl;
    float Ns;
    XMFLOAT3 Ka;
    XMFLOAT3 Kd;
    XMFLOAT3 Ks;
    XMFLOAT3 Ke;
    float Ni;
    float d;
    float illum;
    string map_Bump;
    string map_Kd;
    string map_Ks;
    string map_Ke;
};