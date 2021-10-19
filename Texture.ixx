module;
#include <string>
#include <d3d12.h>
#include <wrl/client.h>
export module Texture;

using namespace std;
using namespace Microsoft::WRL;

export struct Texture {
	string Name;

	wstring Filename;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
