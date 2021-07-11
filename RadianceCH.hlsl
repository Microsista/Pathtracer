#include "Raytracing.hlsl"

[shader("closesthit")]
void MyClosestHitShader_Triangle(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    PrimitiveMaterialBuffer material;
    material.Kd = l_materialCB.albedo.xyz;
    material.Ks = float3(l_materialCB.metalness, l_materialCB.metalness, l_materialCB.metalness);
    material.Kr = float3(l_materialCB.reflectanceCoef, l_materialCB.reflectanceCoef, l_materialCB.reflectanceCoef);
    /*const float3 Kt = material.Kt;*/
    material.roughness = l_materialCB.roughness;

    uint startIndex = PrimitiveIndex() * 3;
    const uint3 indices = { l_indices[startIndex], l_indices[startIndex + 1], l_indices[startIndex + 2] };

    // Retrieve corresponding vertex normals for the triangle vertices.
    VertexPositionNormalTextureTangent vertices[3] = {
        l_vertices[indices[0]],
        l_vertices[indices[1]],
        l_vertices[indices[2]]
    };

    float3 normals[3] = { vertices[0].normal, vertices[1].normal, vertices[2].normal };
    float3 localNormal = HitAttribute(normals, attr);

    float2 vertexTexCoords[3] = { vertices[0].textureCoordinate, vertices[1].textureCoordinate, vertices[2].textureCoordinate };
    float2 texCoord = HitAttribute(vertexTexCoords, attr);

    float3 texSample = l_texDiffuse.SampleLevel(LinearWrapSampler, texCoord, 0).xyz;
    material.Kd = texSample;

    float3 normalSample = g_normalMap.SampleLevel(LinearWrapSampler, texCoord, 0).xyz;
    if (length(normalSample) > 0.9f && length(normalSample < 1.1f)) {
        localNormal = normalSample;
        localNormal = normalize(localNormal * 2.0 - 1.0);
    }

    float3 specularSample = g_specularMap.SampleLevel(LinearWrapSampler, texCoord, 0).xyz;
    material.Ks = specularSample;

    float3 emittanceSample = g_emissiveMap.SampleLevel(LinearWrapSampler, texCoord, 0).xyz;
    material.Ke = emittanceSample;

    float orientation = HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE ? 1 : -1;
    localNormal *= orientation;

    float3 hitPosition = HitWorldPosition();
    float depth = length(g_sceneCB.cameraPosition - hitPosition) / 200;
    /*float c1 = 3.0f, c2 = 3.0f;
    float attenuation = 1.0f / (1.0f + c1 * depth + c2 * depth * depth);*/
    Info info = Shade(hitPosition, rayPayload, localNormal, material);
    rayPayload.color = info.color /** attenuation*/;
    rayPayload.inShadow = info.inShadow;
    rayPayload.depth = depth;
    rayPayload.prevHitPosition = hitPosition;
}