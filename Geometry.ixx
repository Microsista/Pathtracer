#include <cstdint>
#include <DirectXMath.h>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <stdlib.h>
#include <sstream>
#include <iomanip>

#include <list>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atlbase.h>
#include <assert.h>
#include <array>
#include <algorithm>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <atlbase.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <iostream>
#include <fstream>

#include <wrl/event.h>
#include <ResourceUploadBatch.h>

#include "d3dx12.h"

#include <algorithm>
#include "Math.h"

export module Geometry;

import Math;

using namespace DirectX;
using namespace std;

void print2(double var) {
	std::ostringstream ss;
	ss << var;
	std::string s(ss.str());
	s += "\n";

	OutputDebugStringA(s.c_str());
}

void print2(std::string str) {
	std::ostringstream ss;
	ss << str;
	std::string s(ss.str());
	s += "\n";

	OutputDebugStringA(s.c_str());
}


import DXSampleHelper;

export class GeometryGenerator
{
public:

	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;

	struct Vertex
	{
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			TangentU(t),
			TexC(uv) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TangentU(tx, ty, tz),
			TexC(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT2 TexC;
	};




	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32> Indices32;
		Material Material;

		MeshData() {}

		MeshData(std::vector<Vertex> vert, std::vector<uint32> ind, ::Material mat) {
			Vertices = vert;
			Indices32 = ind;
			Material = mat;
		}

		std::vector<uint16>& GetIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (size_t i = 0; i < Indices32.size(); ++i)
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
			}

			return mIndices16;
		}

	private:
		std::vector<uint16> mIndices16;
	};

	MeshData processMesh(aiMesh* mesh, const aiScene* scene)
	{
		vector<Vertex> vertices;
		vector<unsigned int> indices;
		vector<AssimpTexture> textures;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			// process vertex positions, normals and texture coordinates
			XMFLOAT3 vector;
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = vector;

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.Normal = vector;

			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				XMFLOAT2 vec;
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexC = vec;
			}
			else
				vertex.TexC = XMFLOAT2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}
		// process indices
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		Material mat;


		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			aiColor3D color;
			ai_real real;

			// Read mtl file vertex data
			material->Get(AI_MATKEY_COLOR_AMBIENT, color);
			mat.Ka = XMFLOAT3(color.r, color.g, color.b);
			mat.Ka = XMFLOAT3(0, 0, 0);



			material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			mat.Kd = XMFLOAT3(color.r, color.g, color.b); // albedo


			material->Get(AI_MATKEY_COLOR_SPECULAR, color);
			mat.Ks = XMFLOAT3(color.r, color.g, color.b); // metalness, F0
			mat.Ks = XMFLOAT3(color.r / 5, color.g / 5, color.b / 5); // metalness, F0

			material->Get(AI_MATKEY_SHININESS, real); // roughness
			mat.Ns = 1 - (real / 100);

			//material->Get(AI_MATKEY_TEXTURE_DIFFUSE, 1, 1, )


			aiString path;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
			mat.map_Kd = string(path.C_Str());

			aiString path2;
			material->GetTexture(aiTextureType_NORMALS, 0, &path2);
			mat.map_Bump = string(path2.C_Str());

			//print(path2.C_Str());

			aiString path3;
			material->GetTexture(aiTextureType_SPECULAR, 0, &path3);
			mat.map_Ks = string(path3.C_Str());

			//print(path3.C_Str());

			aiString path4;
			material->GetTexture(aiTextureType_EMISSIVE, 0, &path4);
			mat.map_Ke = string(path4.C_Str());

			//print(path4.C_Str());

			mat.id = mesh->mMaterialIndex;
		}


		return MeshData(vertices, indices/*, textures*/, mat);
	}

	void processNode(aiNode* node, const aiScene* scene)
	{
		// process all the node's meshes (if any)
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(processMesh(mesh, scene));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene);
		}
	}

	void loadModel(std::string path, unsigned int flags)
	{
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(path, flags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
			string errstring("ERROR::ASSIMP::");
			errstring.append(import.GetErrorString());
			OutputDebugStringA(errstring.c_str());
			return;
		}
		//directory = path.substr(0, path.find_last_of('/'));

		processNode(scene->mRootNode, scene);
	}

	std::vector<GeometryGenerator::MeshData> LoadModel(std::string path, unsigned int flags) {
		loadModel(path, flags);

		return meshes;
	}

	std::vector<MeshData> meshes;

	MeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions)
	{
		MeshData meshData;

		//
		// Create the vertices.
		//

		Vertex v[24];

		float w2 = 0.5f * width;
		float h2 = 0.5f * height;
		float d2 = 0.5f * depth;

		// Fill in the front face vertex data.
		v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the back face vertex data.
		v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the top face vertex data.
		v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the bottom face vertex data.
		v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the left face vertex data.
		v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in the right face vertex data.
		v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		meshData.Vertices.assign(&v[0], &v[24]);

		//
		// Create the indices.
		//

		uint32 i[36];

		// Fill in the front face index data
		i[0] = 0; i[1] = 1; i[2] = 2;
		i[3] = 0; i[4] = 2; i[5] = 3;

		// Fill in the back face index data
		i[6] = 4; i[7] = 5; i[8] = 6;
		i[9] = 4; i[10] = 6; i[11] = 7;

		// Fill in the top face index data
		i[12] = 8; i[13] = 9; i[14] = 10;
		i[15] = 8; i[16] = 10; i[17] = 11;

		// Fill in the bottom face index data
		i[18] = 12; i[19] = 13; i[20] = 14;
		i[21] = 12; i[22] = 14; i[23] = 15;

		// Fill in the left face index data
		i[24] = 16; i[25] = 17; i[26] = 18;
		i[27] = 16; i[28] = 18; i[29] = 19;

		// Fill in the right face index data
		i[30] = 20; i[31] = 21; i[32] = 22;
		i[33] = 20; i[34] = 22; i[35] = 23;

		meshData.Indices32.assign(&i[0], &i[36]);

		// Put a cap on the number of subdivisions.
		numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

		for (uint32 i = 0; i < numSubdivisions; ++i)
			Subdivide(meshData);

		return meshData;
	}
	MeshData CreateRoom(float width, float height, float depth)
	{
		MeshData meshData;

		//
		// Create the vertices.
		//

		Vertex v[28];

		float w2 = 0.5f * width;
		float h2 = 0.5f * height;
		float d2 = 0.5f * depth;

		// ground
		v[0] = Vertex(-w2, -h2, -d2, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[1] = Vertex(+w2, -h2, -d2, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[2] = Vertex(+w2, -h2, +d2, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[3] = Vertex(-w2, -h2, +d2, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// roof
		v[4] = Vertex(-w2, +h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5] = Vertex(+w2, +h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6] = Vertex(+w2, +h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7] = Vertex(-w2, +h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// back wall
		v[8] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[9] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[10] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[11] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// left wall
		v[12] = Vertex(-w2, -h2, -d2, 1.0f, .0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);
		v[13] = Vertex(-w2, -h2, +d2, 1.0f, .0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[14] = Vertex(-w2, +h2, -d2, 1.0f, .0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[15] = Vertex(-w2, +h2, +d2, 1.0f, .0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);

		// right wall
		v[16] = Vertex(+w2, -h2, -d2, -1.0f, .0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		v[17] = Vertex(+w2, -h2, +d2, -1.0f, .0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[18] = Vertex(+w2, +h2, -d2, -1.0f, .0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[19] = Vertex(+w2, +h2, +d2, -1.0f, .0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

		// front wall
		v[20] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[21] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[22] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[23] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[24] = Vertex(0.35f * 2 * w2 - w2, 0.25f * 2 * h2 - h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[25] = Vertex(0.1f * 2 * w2 - w2, 0.25f * 2 * h2 - h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[26] = Vertex(0.35f * 2 * w2 - w2, 0.75f * 2 * h2 - h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[27] = Vertex(0.1f * 2 * w2 - w2, 0.75f * 2 * h2 - h2, +d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

		meshData.Vertices.assign(&v[0], &v[28]);

		//
		// Create the indices.
		//

		uint32 i[54];

		// ground
		i[0] = 3; i[1] = 1; i[2] = 0;
		i[3] = 2; i[4] = 1; i[5] = 3;

		// roof
		i[6] = 4; i[7] = 5; i[8] = 7;
		i[9] = 5; i[10] = 6; i[11] = 7;

		// back wall
		i[12] = 8; i[13] = 9; i[14] = 10;
		i[15] = 9; i[16] = 11; i[17] = 10;

		// left wall
		i[18] = 14; i[19] = 13; i[20] = 12;
		i[21] = 14; i[22] = 15; i[23] = 13;

		// right wall
		i[24] = 16; i[25] = 17; i[26] = 18;
		i[27] = 17; i[28] = 19; i[29] = 18;

		// front wall
		i[30] = 20; i[31] = 21; i[32] = 24;
		i[33] = 21; i[34] = 25; i[35] = 24;
		i[36] = 25; i[37] = 21; i[38] = 23;
		i[39] = 27; i[40] = 25; i[41] = 23;
		i[42] = 23; i[43] = 26; i[44] = 27;
		i[45] = 23; i[46] = 22; i[47] = 26;
		i[48] = 22; i[49] = 24; i[50] = 26;
		i[51] = 22; i[52] = 20; i[53] = 24;


		meshData.Indices32.assign(&i[0], &i[54]);

		return meshData;;
	}

	MeshData CreateSkull(float width, float height, float depth)
	{
		MeshData meshData;

		TCHAR buffer[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
		wstring path = std::wstring(buffer).substr(0, pos);

		string s(path.begin(), path.end());
		s.append("\\..\\..\\Models\\skull.txt");

		std::ifstream fin(s);
		//std::ifstream fin("../../Models/skull.txt");

		if (!fin)
		{
			MessageBox(0, L"Models/skull.txt not found.", 0, 0);
			return meshData;
		}

		UINT vcount = 0;
		UINT tcount = 0;
		std::string ignore;

		fin >> ignore >> vcount;
		fin >> ignore >> tcount;
		fin >> ignore >> ignore >> ignore >> ignore;

		XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
		XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

		XMVECTOR vMin = XMLoadFloat3(&vMinf3);
		XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

		std::vector<Vertex> vertices(vcount);
		for (UINT i = 0; i < vcount; ++i)
		{
			/*fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
			fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;*/

			fin >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
			fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

			XMVECTOR P = XMLoadFloat3(&vertices[i].Position);

			// Project point onto unit sphere and generate spherical texture coordinates.
			//XMFLOAT3 spherePos;
			//XMStoreFloat3(&spherePos, XMVector3Normalize(P));

			//float theta = atan2f(spherePos.z, spherePos.x);

			//// Put in [0, 2pi].
			//if (theta < 0.0f)
			//    theta += XM_2PI;

			//float phi = acosf(spherePos.y);

			//float u = theta / (2.0f * XM_PI);
			//float v = phi / XM_PI;

			//vertices[i].TexC = { u, v };

			//vMin = XMVectorMin(vMin, P);
			//vMax = XMVectorMax(vMax, P);
		}

		/*  BoundingBox bounds;
		XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
		XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));*/

		fin >> ignore;
		fin >> ignore;
		fin >> ignore;

		std::vector<std::uint32_t> indices(3 * tcount);
		for (UINT i = 0; i < tcount; ++i)
		{
			fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
		}

		fin.close();

		meshData.Vertices = vertices;
		meshData.Indices32 = indices;


		return meshData;
	}
	MeshData CreateCoordinates(float width, float height, float depth)
	{
		MeshData meshData;

		//
		// Create the vertices.
		//

		Vertex v[24 * 3];

		float w2 = 0.5f * width;
		float h2 = 0.5f * height;
		float d2 = 0.5f * depth;

		// X
		// Fill in the front face vertex data.
		v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the back face vertex data.
		v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the top face vertex data.
		v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the bottom face vertex data.
		v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the left face vertex data.
		v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in the right face vertex data.
		v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		// Y
		// Fill in twe front face verteh data.
		v[0 + 24] = Vertex(-h2, -w2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1 + 24] = Vertex(-h2, +w2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2 + 24] = Vertex(+h2, +w2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3 + 24] = Vertex(+h2, -w2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in twe back face Vertex data.
		v[4 + 24] = Vertex(-h2, -w2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5 + 24] = Vertex(+h2, -w2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6 + 24] = Vertex(+h2, +w2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7 + 24] = Vertex(-h2, +w2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in twe top face Vertex data.
		v[8 + 24] = Vertex(-h2, +w2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9 + 24] = Vertex(-h2, +w2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10 + 24] = Vertex(+h2, +w2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11 + 24] = Vertex(+h2, +w2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in twe bottom face Vertex data.
		v[12 + 24] = Vertex(-h2, -w2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13 + 24] = Vertex(+h2, -w2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14 + 24] = Vertex(+h2, -w2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15 + 24] = Vertex(-h2, -w2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in twe left face Vertex data.
		v[16 + 24] = Vertex(-h2, -w2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17 + 24] = Vertex(-h2, +w2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18 + 24] = Vertex(-h2, +w2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19 + 24] = Vertex(-h2, -w2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in twe rigwt face Vertex data.
		v[20 + 24] = Vertex(+h2, -w2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21 + 24] = Vertex(+h2, +w2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22 + 24] = Vertex(+h2, +w2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23 + 24] = Vertex(+h2, -w2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		// Z
		// Fill in the front face vertex data.
		v[0 + 48] = Vertex(-d2, -h2, -w2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1 + 48] = Vertex(-d2, +h2, -w2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2 + 48] = Vertex(+d2, +h2, -w2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3 + 48] = Vertex(+d2, -h2, -w2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the back face Vertex wata.
		v[4 + 48] = Vertex(-d2, -h2, +w2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5 + 48] = Vertex(+d2, -h2, +w2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6 + 48] = Vertex(+d2, +h2, +w2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7 + 48] = Vertex(-d2, +h2, +w2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the top face Vertex wata.
		v[8 + 48] = Vertex(-d2, +h2, -w2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9 + 48] = Vertex(-d2, +h2, +w2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10 + 48] = Vertex(+d2, +h2, +w2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11 + 48] = Vertex(+d2, +h2, -w2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the bottom face Vertex wata.
		v[12 + 48] = Vertex(-d2, -h2, -w2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13 + 48] = Vertex(+d2, -h2, -w2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14 + 48] = Vertex(+d2, -h2, +w2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15 + 48] = Vertex(-d2, -h2, +w2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the left face Vertex wata.
		v[16 + 48] = Vertex(-d2, -h2, +w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17 + 48] = Vertex(-d2, +h2, +w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18 + 48] = Vertex(-d2, +h2, -w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19 + 48] = Vertex(-d2, -h2, -w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in the right face Vertex wata.
		v[20 + 48] = Vertex(+d2, -h2, -w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21 + 48] = Vertex(+d2, +h2, -w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22 + 48] = Vertex(+d2, +h2, +w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23 + 48] = Vertex(+d2, -h2, +w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		meshData.Vertices.assign(&v[0], &v[24 * 3]);

		//
		// Create the indices.
		//

		uint32 i[36 * 3];

		//x
		// Fill in the front face index data
		i[0] = 0; i[1] = 1; i[2] = 2;
		i[3] = 0; i[4] = 2; i[5] = 3;

		// Fill in the back face index data
		i[6] = 4; i[7] = 5; i[8] = 6;
		i[9] = 4; i[10] = 6; i[11] = 7;

		// Fill in the top face index data
		i[12] = 8; i[13] = 9; i[14] = 10;
		i[15] = 8; i[16] = 10; i[17] = 11;

		// Fill in the bottom face index data
		i[18] = 12; i[19] = 13; i[20] = 14;
		i[21] = 12; i[22] = 14; i[23] = 15;

		// Fill in the left face index data
		i[24] = 16; i[25] = 17; i[26] = 18;
		i[27] = 16; i[28] = 18; i[29] = 19;

		// Fill in the right face index data
		i[30] = 20; i[31] = 21; i[32] = 22;
		i[33] = 20; i[34] = 22; i[35] = 23;

		//y
		// Fill in the front face index data
		i[0 + 36] = 24 + 0; i[1 + 36] = 24 + 1; i[2 + 36] = 24 + 2;
		i[3 + 36] = 24 + 0; i[4 + 36] = 24 + 2; i[5 + 36] = 24 + 3;

		// Fill in the back face index data
		i[6 + 36] = 24 + 4; i[7 + 36] = 24 + 5; i[8 + 36] = 24 + 6;
		i[9 + 36] = 24 + 4; i[10 + 36] = 24 + 6; i[11 + 36] = 24 + 7;

		// Fill in the top face index data
		i[12 + 36] = 24 + 8; i[13 + 36] = 24 + 9; i[14 + 36] = 24 + 10;
		i[15 + 36] = 24 + 8; i[16 + 36] = 24 + 10; i[17 + 36] = 24 + 11;

		// Fill in the bottom face index data
		i[18 + 36] = 24 + 12; i[19 + 36] = 24 + 13; i[20 + 36] = 24 + 14;
		i[21 + 36] = 24 + 12; i[22 + 36] = 24 + 14; i[23 + 36] = 24 + 15;

		// Fill in the left face index data
		i[24 + 36] = 24 + 16; i[25 + 36] = 24 + 17; i[26 + 36] = 24 + 18;
		i[27 + 36] = 24 + 16; i[28 + 36] = 24 + 18; i[29 + 36] = 24 + 19;

		// Fill in the right face index data
		i[30 + 36] = 24 + 20; i[31 + 36] = 24 + 21; i[32 + 36] = 24 + 22;
		i[33 + 36] = 24 + 20; i[34 + 36] = 24 + 22; i[35 + 36] = 24 + 23;

		//z
		// Fill in the front face index data
		i[0 + 72] = 48 + 0; i[1 + 72] = 48 + 1; i[2 + 72] = 48 + 2;
		i[3 + 72] = 48 + 0; i[4 + 72] = 48 + 2; i[5 + 72] = 48 + 3;

		// Fill in the back face index data
		i[6 + 72] = 48 + 4; i[7 + 72] = 48 + 5; i[8 + 72] = 48 + 6;
		i[9 + 72] = 48 + 4; i[10 + 72] = 48 + 6; i[11 + 72] = 48 + 7;

		// Fill in the top face index data
		i[12 + 72] = 48 + 8; i[13 + 72] = 48 + 9; i[14 + 72] = 48 + 10;
		i[15 + 72] = 48 + 8; i[16 + 72] = 48 + 10; i[17 + 72] = 48 + 11;

		// Fill in the bottom face index data
		i[18 + 72] = 48 + 12; i[19 + 72] = 48 + 13; i[20 + 72] = 48 + 14;
		i[21 + 72] = 48 + 12; i[22 + 72] = 48 + 14; i[23 + 72] = 48 + 15;

		// Fill in the left face index data
		i[24 + 72] = 48 + 16; i[25 + 72] = 48 + 17; i[26 + 72] = 48 + 18;
		i[27 + 72] = 48 + 16; i[28 + 72] = 48 + 18; i[29 + 72] = 48 + 19;

		// Fill in the right face index data
		i[30 + 72] = 48 + 20; i[31 + 72] = 48 + 21; i[32 + 72] = 48 + 22;
		i[33 + 72] = 48 + 20; i[34 + 72] = 48 + 22; i[35 + 72] = 48 + 23;

		meshData.Indices32.assign(&i[0], &i[36 * 3]);

		return meshData;
	}
	MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
	{
		MeshData meshData;

		//
		// Compute the vertices stating at the top pole and moving down the stacks.
		//

		// Poles: note that there will be texture coordinate distortion as there is
		// not a unique point on the texture map to assign to the pole when mapping
		// a rectangular texture onto a sphere.
		Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

		meshData.Vertices.push_back(topVertex);

		float phiStep = XM_PI / stackCount;
		float thetaStep = 2.0f * XM_PI / sliceCount;

		// Compute vertices for each stack ring (do not count the poles as rings).
		for (uint32 i = 1; i <= stackCount - 1; ++i)
		{
			float phi = i * phiStep;

			// Vertices of ring.
			for (uint32 j = 0; j <= sliceCount; ++j)
			{
				float theta = j * thetaStep;

				Vertex v;

				// spherical to cartesian
				v.Position.x = radius * sinf(phi) * cosf(theta);
				v.Position.y = radius * cosf(phi);
				v.Position.z = radius * sinf(phi) * sinf(theta);

				// Partial derivative of P with respect to theta
				v.TangentU.x = -radius * sinf(phi) * sinf(theta);
				v.TangentU.y = 0.0f;
				v.TangentU.z = +radius * sinf(phi) * cosf(theta);

				XMVECTOR T = XMLoadFloat3(&v.TangentU);
				XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

				XMVECTOR p = XMLoadFloat3(&v.Position);
				XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

				v.TexC.x = theta / XM_2PI;
				v.TexC.y = phi / XM_PI;

				meshData.Vertices.push_back(v);
			}
		}

		meshData.Vertices.push_back(bottomVertex);

		//
		// Compute indices for top stack.  The top stack was written first to the vertex buffer
		// and connects the top pole to the first ring.
		//

		for (uint32 i = 1; i <= sliceCount; ++i)
		{
			meshData.Indices32.push_back(0);
			meshData.Indices32.push_back(i + 1);
			meshData.Indices32.push_back(i);
		}

		//
		// Compute indices for inner stacks (not connected to poles).
		//

		// Offset the indices to the index of the first vertex in the first ring.
		// This is just skipping the top pole vertex.
		uint32 baseIndex = 1;
		uint32 ringVertexCount = sliceCount + 1;
		for (uint32 i = 0; i < stackCount - 2; ++i)
		{
			for (uint32 j = 0; j < sliceCount; ++j)
			{
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j);
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
			}
		}

		//
		// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
		// and connects the bottom pole to the bottom ring.
		//

		// South pole vertex was added last.
		uint32 southPoleIndex = (uint32)meshData.Vertices.size() - 1;

		// Offset the indices to the index of the first vertex in the last ring.
		baseIndex = southPoleIndex - ringVertexCount;

		for (uint32 i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(southPoleIndex);
			meshData.Indices32.push_back(baseIndex + i);
			meshData.Indices32.push_back(baseIndex + i + 1);
		}

		return meshData;
	}
	MeshData CreateGeosphere(float radius, uint32 numSubdivisions)
	{
		MeshData meshData;

		// Put a cap on the number of subdivisions.
		numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

		// Approximate a sphere by tessellating an icosahedron.

		const float X = 0.525731f;
		const float Z = 0.850651f;

		XMFLOAT3 pos[12] =
		{
			XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),
			XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),
			XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X),
			XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),
			XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f),
			XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
		};

		uint32 k[60] =
		{
			1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
			1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
			3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
			10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
		};

		meshData.Vertices.resize(12);
		meshData.Indices32.assign(&k[0], &k[60]);

		for (uint32 i = 0; i < 12; ++i)
			meshData.Vertices[i].Position = pos[i];

		for (uint32 i = 0; i < numSubdivisions; ++i)
			Subdivide(meshData);

		// Project vertices onto sphere and scale.
		for (uint32 i = 0; i < meshData.Vertices.size(); ++i)
		{
			// Project onto unit sphere.
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&meshData.Vertices[i].Position));

			// Project onto sphere.
			XMVECTOR p = radius * n;

			XMStoreFloat3(&meshData.Vertices[i].Position, p);
			XMStoreFloat3(&meshData.Vertices[i].Normal, n);

			// Derive texture coordinates from spherical coordinates.
			float theta = atan2f(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

			// Put in [0, 2pi].
			if (theta < 0.0f)
				theta += XM_2PI;

			float phi = acosf(meshData.Vertices[i].Position.y / radius);

			meshData.Vertices[i].TexC.x = theta / XM_2PI;
			meshData.Vertices[i].TexC.y = phi / XM_PI;

			// Partial derivative of P with respect to theta
			meshData.Vertices[i].TangentU.x = -radius * sinf(phi) * sinf(theta);
			meshData.Vertices[i].TangentU.y = 0.0f;
			meshData.Vertices[i].TangentU.z = +radius * sinf(phi) * cosf(theta);

			XMVECTOR T = XMLoadFloat3(&meshData.Vertices[i].TangentU);
			XMStoreFloat3(&meshData.Vertices[i].TangentU, XMVector3Normalize(T));
		}

		return meshData;
	}
	MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
	{
		MeshData meshData;

		//
		// Build Stacks.
		// 

		float stackHeight = height / stackCount;

		// Amount to increment radius as we move up each stack level from bottom to top.
		float radiusStep = (topRadius - bottomRadius) / stackCount;

		uint32 ringCount = stackCount + 1;

		// Compute vertices for each stack ring starting at the bottom and moving up.
		for (uint32 i = 0; i < ringCount; ++i)
		{
			float y = -0.5f * height + i * stackHeight;
			float r = bottomRadius + i * radiusStep;

			// vertices of ring
			float dTheta = 2.0f * XM_PI / sliceCount;
			for (uint32 j = 0; j <= sliceCount; ++j)
			{
				Vertex vertex;

				float c = cosf(j * dTheta);
				float s = sinf(j * dTheta);

				vertex.Position = XMFLOAT3(r * c, y, r * s);

				vertex.TexC.x = (float)j / sliceCount;
				vertex.TexC.y = 1.0f - (float)i / stackCount;

				// Cylinder can be parameterized as follows, where we introduce v
				// parameter that goes in the same direction as the v tex-coord
				// so that the bitangent goes in the same direction as the v tex-coord.
				//   Let r0 be the bottom radius and let r1 be the top radius.
				//   y(v) = h - hv for v in [0,1].
				//   r(v) = r1 + (r0-r1)v
				//
				//   x(t, v) = r(v)*cos(t)
				//   y(t, v) = h - hv
				//   z(t, v) = r(v)*sin(t)
				// 
				//  dx/dt = -r(v)*sin(t)
				//  dy/dt = 0
				//  dz/dt = +r(v)*cos(t)
				//
				//  dx/dv = (r0-r1)*cos(t)
				//  dy/dv = -h
				//  dz/dv = (r0-r1)*sin(t)

				// This is unit length.
				vertex.TangentU = XMFLOAT3(-s, 0.0f, c);

				float dr = bottomRadius - topRadius;
				XMFLOAT3 bitangent(dr * c, -height, dr * s);

				XMVECTOR T = XMLoadFloat3(&vertex.TangentU);
				XMVECTOR B = XMLoadFloat3(&bitangent);
				XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
				XMStoreFloat3(&vertex.Normal, N);

				meshData.Vertices.push_back(vertex);
			}
		}

		// Add one because we duplicate the first and last vertex per ring
		// since the texture coordinates are different.
		uint32 ringVertexCount = sliceCount + 1;

		// Compute indices for each stack.
		for (uint32 i = 0; i < stackCount; ++i)
		{
			for (uint32 j = 0; j < sliceCount; ++j)
			{
				meshData.Indices32.push_back(i * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);

				meshData.Indices32.push_back(i * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
				meshData.Indices32.push_back(i * ringVertexCount + j + 1);
			}
		}

		BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
		BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

		return meshData;
	}
	MeshData CreateGrid(float width, float depth, uint32 m, uint32 n)
	{
		MeshData meshData;

		uint32 vertexCount = m * n;
		uint32 faceCount = (m - 1) * (n - 1) * 2;

		//
		// Create the vertices.
		//

		float halfWidth = 0.5f * width;
		float halfDepth = 0.5f * depth;

		float dx = width / (n - 1);
		float dz = depth / (m - 1);

		float du = 1.0f / (n - 1);
		float dv = 1.0f / (m - 1);

		meshData.Vertices.resize(vertexCount);
		for (uint32 i = 0; i < m; ++i)
		{
			float z = halfDepth - i * dz;
			for (uint32 j = 0; j < n; ++j)
			{
				float x = -halfWidth + j * dx;

				meshData.Vertices[i * n + j].Position = XMFLOAT3(x, 0.0f, z);
				meshData.Vertices[i * n + j].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
				meshData.Vertices[i * n + j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

				// Stretch texture over grid.
				meshData.Vertices[i * n + j].TexC.x = j * du;
				meshData.Vertices[i * n + j].TexC.y = i * dv;
			}
		}

		//
		// Create the indices.
		//

		meshData.Indices32.resize(faceCount * 3); // 3 indices per face

												  // Iterate over each quad and compute indices.
		uint32 k = 0;
		for (uint32 i = 0; i < m - 1; ++i)
		{
			for (uint32 j = 0; j < n - 1; ++j)
			{
				meshData.Indices32[k] = i * n + j;
				meshData.Indices32[k + 1] = i * n + j + 1;
				meshData.Indices32[k + 2] = (i + 1) * n + j;

				meshData.Indices32[k + 3] = (i + 1) * n + j;
				meshData.Indices32[k + 4] = i * n + j + 1;
				meshData.Indices32[k + 5] = (i + 1) * n + j + 1;

				k += 6; // next quad
			}
		}

		return meshData;
	}
	MeshData CreateQuad(float x, float y, float w, float h, float depth)
	{
		MeshData meshData;

		meshData.Vertices.resize(4);
		meshData.Indices32.resize(6);

		// Position coordinates specified in NDC space.
		meshData.Vertices[0] = Vertex(
			x, y - h, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 1.0f);

		meshData.Vertices[1] = Vertex(
			x, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 0.0f);

		meshData.Vertices[2] = Vertex(
			x + w, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f);

		meshData.Vertices[3] = Vertex(
			x + w, y - h, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 1.0f);

		meshData.Indices32[0] = 0;
		meshData.Indices32[1] = 1;
		meshData.Indices32[2] = 2;

		meshData.Indices32[3] = 0;
		meshData.Indices32[4] = 2;
		meshData.Indices32[5] = 3;

		return meshData;
	}

private:
	void Subdivide(MeshData& meshData)
	{
		// Save a copy of the input geometry.
		MeshData inputCopy = meshData;


		meshData.Vertices.resize(0);
		meshData.Indices32.resize(0);

		//       v1
		//       *
		//      / \
			//     /   \
	//  m0*-----*m1
//   / \   / \
	//  /   \ /   \
	// *-----*-----*
// v0    m2     v2

		uint32 numTris = (uint32)inputCopy.Indices32.size() / 3;
		for (uint32 i = 0; i < numTris; ++i)
		{
			Vertex v0 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 0]];
			Vertex v1 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 1]];
			Vertex v2 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 2]];

			//
			// Generate the midpoints.
			//

			Vertex m0 = MidPoint(v0, v1);
			Vertex m1 = MidPoint(v1, v2);
			Vertex m2 = MidPoint(v0, v2);

			//
			// Add new geometry.
			//

			meshData.Vertices.push_back(v0); // 0
			meshData.Vertices.push_back(v1); // 1
			meshData.Vertices.push_back(v2); // 2
			meshData.Vertices.push_back(m0); // 3
			meshData.Vertices.push_back(m1); // 4
			meshData.Vertices.push_back(m2); // 5

			meshData.Indices32.push_back(i * 6 + 0);
			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 5);

			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 4);
			meshData.Indices32.push_back(i * 6 + 5);

			meshData.Indices32.push_back(i * 6 + 5);
			meshData.Indices32.push_back(i * 6 + 4);
			meshData.Indices32.push_back(i * 6 + 2);

			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 1);
			meshData.Indices32.push_back(i * 6 + 4);
		}
	}
	Vertex MidPoint(const Vertex& v0, const Vertex& v1)
	{
		XMVECTOR p0 = XMLoadFloat3(&v0.Position);
		XMVECTOR p1 = XMLoadFloat3(&v1.Position);

		XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
		XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

		XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
		XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

		XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
		XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

		// Compute the midpoints of all the attributes.  Vectors need to be normalized
		// since linear interpolating can make them not unit length.  
		XMVECTOR pos = 0.5f * (p0 + p1);
		XMVECTOR normal = XMVector3Normalize(0.5f * (n0 + n1));
		XMVECTOR tangent = XMVector3Normalize(0.5f * (tan0 + tan1));
		XMVECTOR tex = 0.5f * (tex0 + tex1);

		Vertex v;
		XMStoreFloat3(&v.Position, pos);
		XMStoreFloat3(&v.Normal, normal);
		XMStoreFloat3(&v.TangentU, tangent);
		XMStoreFloat2(&v.TexC, tex);

		return v;
	}
	void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
	{
		uint32 baseIndex = (uint32)meshData.Vertices.size();

		float y = 0.5f * height;
		float dTheta = 2.0f * XM_PI / sliceCount;

		// Duplicate cap ring vertices because the texture coordinates and normals differ.
		for (uint32 i = 0; i <= sliceCount; ++i)
		{
			float x = topRadius * cosf(i * dTheta);
			float z = topRadius * sinf(i * dTheta);

			// Scale down by the height to try and make top cap texture coord area
			// proportional to base.
			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
		}

		// Cap center vertex.
		meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

		// Index of center vertex.
		uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

		for (uint32 i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(centerIndex);
			meshData.Indices32.push_back(baseIndex + i + 1);
			meshData.Indices32.push_back(baseIndex + i);
		}
	}
	void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
	{
		// 
		// Build bottom cap.
		//

		uint32 baseIndex = (uint32)meshData.Vertices.size();
		float y = -0.5f * height;

		// vertices of ring
		float dTheta = 2.0f * XM_PI / sliceCount;
		for (uint32 i = 0; i <= sliceCount; ++i)
		{
			float x = bottomRadius * cosf(i * dTheta);
			float z = bottomRadius * sinf(i * dTheta);

			// Scale down by the height to try and make top cap texture coord area
			// proportional to base.
			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
		}

		// Cap center vertex.
		meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

		// Cache the index of center vertex.
		uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

		for (uint32 i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(centerIndex);
			meshData.Indices32.push_back(baseIndex + i);
			meshData.Indices32.push_back(baseIndex + i + 1);
		}
	}
};

