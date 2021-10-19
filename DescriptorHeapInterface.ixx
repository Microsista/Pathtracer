module;
#include <d3d12.h>
export module DescriptorHeapInterface;

export struct DescriptorHeapInterface {
	virtual UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX) = 0;
	virtual ID3D12DescriptorHeap* GetHeap() = 0;
	virtual UINT DescriptorSize() = 0;
};