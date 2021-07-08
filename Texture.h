#pragma once

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};