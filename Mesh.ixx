module;
#include <d3d12.h>
#include <DirectXCollision.h>
#include <string>
#include <wrl.h>
#include <unordered_map>
export module Mesh;

import Helper;
import DXSampleHelper;

export class MeshGeometry
{
public:
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	D3D12_VERTEX_BUFFER_VIEW ColorBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW cbv;
		cbv.BufferLocation = ColorBufferGPU->GetGPUVirtualAddress();
		cbv.StrideInBytes = ColorByteStride;
		cbv.SizeInBytes = ColorBufferByteSize;

		return cbv;
	}

	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
		ColorBufferUploader = nullptr;
	}

public:
	class Submesh
	{
	public:
		Submesh() = default;
		Submesh(UINT indexCount, UINT startIndexLocation, UINT vertexCount, UINT baseVertexLocation) :
			IndexCount(indexCount), StartIndexLocation(startIndexLocation), VertexCount(vertexCount), BaseVertexLocation(baseVertexLocation) {};
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		UINT VertexCount = 0;
		INT BaseVertexLocation = 0;
		Material Material;

		DirectX::BoundingBox Bounds;
	};

	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> ColorBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferUploader = nullptr;

	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	UINT ColorByteStride = 0;
	UINT ColorBufferByteSize = 0;

	std::unordered_map<std::string, Submesh> DrawArgs;
};