module;
#include <vector>
#include <cstdint>
export module MeshData;

import Vertex;
import Material;

using namespace std;
using namespace Geometry;

export struct MeshData
{
	vector<Vertex> Vertices;
	vector<uint32_t> Indices32;
	Material Material;

	MeshData() {}

	MeshData(std::vector<Vertex> vert, std::vector<uint32_t> ind, ::Material mat) {
		Vertices = vert;
		Indices32 = ind;
		Material = mat;
	}

	vector<uint16_t>& GetIndices16()
	{
		if (mIndices16.empty())
		{
			mIndices16.resize(Indices32.size());
			for (size_t i = 0; i < Indices32.size(); ++i)
				mIndices16[i] = static_cast<uint16_t>(Indices32[i]);
		}

		return mIndices16;
	}

	vector<Vertex> getVertices() {
		return Vertices;
	}

private:
	vector<uint16_t> mIndices16;
};