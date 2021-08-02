#pragma once

#include <DDSTextureLoader.h>

using Microsoft::WRL::ComPtr;

inline void print(double var) {
    std::ostringstream ss;
    ss << var;
    std::string s(ss.str());
    s += "\n";

    OutputDebugStringA(s.c_str());
}

inline void print(std::string str) {
    std::ostringstream ss;
    ss << str;
    std::string s(ss.str());
    s += "\n";

    OutputDebugStringA(s.c_str());
}

class HrException : public std::runtime_error
{
    inline std::string HrToString(HRESULT hr)
    {
        char s_str[64] = {};
        sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
        return std::string(s_str);
    }
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        OutputDebugString(msg);
        throw HrException(hr);
    }
}

inline void ThrowIfFalse(bool value)
{
    ThrowIfFailed(value ? S_OK : E_FAIL);
}

inline void ThrowIfFalse(bool value, const wchar_t* msg)
{
    ThrowIfFailed(value ? S_OK : E_FAIL, msg);
}


inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        // Method failed or path was truncated.
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}

inline HRESULT ReadDataFromFile(LPCWSTR filename, BYTE** data, UINT* size)
{
    using namespace Microsoft::WRL;

    CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
    extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
    extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    extendedParams.lpSecurityAttributes = nullptr;
    extendedParams.hTemplateFile = nullptr;

    Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams));
    if (file.Get() == INVALID_HANDLE_VALUE)
    {
        throw std::exception();
    }

    FILE_STANDARD_INFO fileInfo = {};
    if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        throw std::exception();
    }

    if (fileInfo.EndOfFile.HighPart != 0)
    {
        throw std::exception();
    }

    *data = reinterpret_cast<BYTE*>(malloc(fileInfo.EndOfFile.LowPart));
    *size = fileInfo.EndOfFile.LowPart;

    if (!ReadFile(file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
    {
        throw std::exception();
    }

    return S_OK;
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

inline UINT Align(UINT size, UINT alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

inline UINT CalculateConstantBufferByteSize(UINT byteSize)
{
    // Constant buffer size is required to be aligned.
    return Align(byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

#ifdef D3D_COMPILE_STANDARD_FILE_INCLUDE
inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}
#endif

// Resets all elements in a ComPtr array.
template<class T>
void ResetComPtrArray(T* comPtrArray)
{
    for (auto &i : *comPtrArray)
    {
        i.Reset();
    }
}


// Resets all elements in a unique_ptr array.
template<class T>
void ResetUniquePtrArray(T* uniquePtrArray)
{
    for (auto &i : *uniquePtrArray)
    {
        i.reset();
    }
}

class GpuUploadBuffer
{
public:
    ComPtr<ID3D12Resource> GetResource() { return m_resource; }
    virtual void Release() { m_resource.Reset(); }
protected:
    ComPtr<ID3D12Resource> m_resource;

    GpuUploadBuffer() {}
    ~GpuUploadBuffer()
    {
        if (m_resource.Get())
        {
            m_resource->Unmap(0, nullptr);
        }
    }

    void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
    {
        auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_resource)));
        m_resource->SetName(resourceName);
    }

    uint8_t* MapCpuWriteOnly()
    {
        uint8_t* mappedData;
        // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
        return mappedData;
    }
};

struct D3DBuffer
{
    ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
};

struct D3DTexture
{
    ComPtr<ID3D12Resource> resource;
    ComPtr<ID3D12Resource> upload;      // TODO: release after initialization
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
    UINT heapIndex = UINT_MAX;
};



// Helper class to create and update a constant buffer with proper constant buffer alignments.
// Usage: 
//    ConstantBuffer<...> cb;
//    cb.Create(...);
//    cb.staging.var = ... ; | cb->var = ... ; 
//    cb.CopyStagingToGPU(...);
//    Set...View(..., cb.GputVirtualAddress());
template <class T>
class ConstantBuffer : public GpuUploadBuffer
{
    uint8_t* m_mappedConstantData;
    UINT m_alignedInstanceSize;
    UINT m_numInstances;

public:
    ConstantBuffer() : m_alignedInstanceSize(0), m_numInstances(0), m_mappedConstantData(nullptr) {}

    void Create(ID3D12Device* device, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
        m_numInstances = numInstances;
        m_alignedInstanceSize = Align(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        UINT bufferSize = numInstances * m_alignedInstanceSize;
        Allocate(device, bufferSize, resourceName);
        m_mappedConstantData = MapCpuWriteOnly();
    }

    void CopyStagingToGpu(UINT instanceIndex = 0)
    {
        memcpy(m_mappedConstantData + instanceIndex * m_alignedInstanceSize, &staging, sizeof(T));
    }

    // Accessors
    T staging;
    T* operator->() { return &staging; }
    UINT NumInstances() { return m_numInstances; }
    D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
    {
        return m_resource->GetGPUVirtualAddress() + instanceIndex * m_alignedInstanceSize;
    }
};


// Helper class to create and update a structured buffer.
// Usage: 
//    StructuredBuffer<...> sb;
//    sb.Create(...);
//    sb[index].var = ... ; 
//    sb.CopyStagingToGPU(...);
//    Set...View(..., sb.GputVirtualAddress());
template <class T>
class StructuredBuffer : public GpuUploadBuffer
{
    T* m_mappedBuffers;
    std::vector<T> m_staging;
    UINT m_numInstances;

public:
    // Performance tip: Align structures on sizeof(float4) boundary.
    // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
    static_assert(sizeof(T) % 16 == 0, L"Align structure buffers on 16 byte boundary for performance reasons.");

    StructuredBuffer() : m_mappedBuffers(nullptr), m_numInstances(0) {}

    void Create(ID3D12Device* device, UINT numElements, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
        m_staging.resize(numElements);
        UINT bufferSize = numInstances * numElements * sizeof(T);
        Allocate(device, bufferSize, resourceName);
        m_mappedBuffers = reinterpret_cast<T*>(MapCpuWriteOnly());
    }

    void CopyStagingToGpu(UINT instanceIndex = 0)
    {
        memcpy(m_mappedBuffers + instanceIndex * NumElementsPerInstance(), &m_staging[0], InstanceSize());
    }

    // Accessors
    T& operator[](UINT elementIndex) { return m_staging[elementIndex]; }
    size_t NumElementsPerInstance() { return m_staging.size(); }
    UINT NumInstances() { return m_staging.size(); }
    size_t InstanceSize() { return NumElementsPerInstance() * sizeof(T); }
    D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
    {
        return m_resource->GetGPUVirtualAddress() + instanceIndex * InstanceSize();
    }
};

namespace DX
{
    class DescriptorHeap
    {
        ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
        UINT m_descriptorsAllocated;
        UINT m_descriptorSize;

    public:
        D3D12_DESCRIPTOR_HEAP_DESC GetDesc() {
            return m_descriptorHeap->GetDesc();
        }
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart()
        {
            return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
        }
        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleForHeapStart()
        {
            return m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        }

        DescriptorHeap(){}

        DescriptorHeap(ID3D12Device5* device, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
        {
            m_descriptorsAllocated = 0;

            D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
            descriptorHeapDesc.NumDescriptors = numDescriptors;
            descriptorHeapDesc.Type = type;
            descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            descriptorHeapDesc.NodeMask = 0;
            device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
            NAME_D3D12_OBJECT(m_descriptorHeap);

            m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        ID3D12DescriptorHeap* GetHeap() { return m_descriptorHeap.Get(); }
        ID3D12DescriptorHeap** GetAddressOf() { return m_descriptorHeap.GetAddressOf(); }
        UINT DescriptorSize() { return m_descriptorSize; }

        // Allocate a descriptor and return its index. 
        // Passing descriptorIndexToUse as UINT_MAX will allocate next available descriptor.
        // Otherwise the descriptorIndexToUse will be used instead of allocating a new one.
        UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX)
        {
            if (descriptorIndexToUse == UINT_MAX)
            {
                ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!");
                descriptorIndexToUse = m_descriptorsAllocated++;
            }
            else
            {
                ThrowIfFalse(descriptorIndexToUse < m_descriptorHeap->GetDesc().NumDescriptors, L"Requested descriptor index is out of bounds!");
                m_descriptorsAllocated = max(descriptorIndexToUse + 1, m_descriptorsAllocated);
            }

            auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
            *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
            return descriptorIndexToUse;
        }

        // Allocate multiple descriptor indices and return an index of a first one. 
        // Passing firstDescriptorIndexToUse as UINT_MAX will allocate next available descriptors.
        // Otherwise the firstDescriptorIndexToUse will be used instead of allocating a new one.
        UINT AllocateDescriptorIndices(UINT numDescriptors, UINT firstDescriptorIndexToUse = UINT_MAX)
        {
            auto handle = D3D12_CPU_DESCRIPTOR_HANDLE();
            firstDescriptorIndexToUse = AllocateDescriptor(&handle, firstDescriptorIndexToUse);

            for (UINT i = 1; i < numDescriptors; i++)
            {
                AllocateDescriptor(&handle, firstDescriptorIndexToUse + i);
            }
            return firstDescriptorIndexToUse;
        }
    };
}

inline void CreateTextureSRV(
    ID3D12Device5* device,
    ID3D12Resource* resource,
    DX::DescriptorHeap* descriptorHeap,
    UINT* descriptorHeapIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
    D3D12_SRV_DIMENSION srvDimension = D3D12_SRV_DIMENSION_TEXTURE2D)
{
    // Describe and create an SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = srvDimension;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = resource->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    *descriptorHeapIndex = descriptorHeap->AllocateDescriptor(cpuHandle, *descriptorHeapIndex);
    device->CreateShaderResourceView(resource, &srvDesc, *cpuHandle);
    *gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetHeap()->GetGPUDescriptorHandleForHeapStart(),
        *descriptorHeapIndex, descriptorHeap->DescriptorSize());
};

// Loads a DDS texture and issues upload on the commandlist. 
// The caller is expected to execute the commandList.
inline void LoadDDSTexture(
    ID3D12Device5* device,
    ID3D12GraphicsCommandList4* commandList,
    const wchar_t* filename,
    DX::DescriptorHeap* descriptorHeap,
    ID3D12Resource** ppResource,
    ID3D12Resource** ppUpload,
    UINT* descriptorHeapIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
    D3D12_SRV_DIMENSION srvDimension = D3D12_SRV_DIMENSION_TEXTURE2D)
{
    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(device, filename, ppResource, ddsData, subresources));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(*ppResource, 0, static_cast<UINT>(subresources.size()));

    // Create the GPU upload buffer.
    ThrowIfFailed(
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(ppUpload)));

    UpdateSubresources(commandList, *ppResource, *ppUpload, 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*ppResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    CreateTextureSRV(device, *ppResource, descriptorHeap, descriptorHeapIndex, cpuHandle, gpuHandle, srvDimension);
}

inline void LoadDDSTexture(
    ID3D12Device5* device,
    ID3D12GraphicsCommandList4* commandList,
    const wchar_t* filename,
    DX::DescriptorHeap* descriptorHeap,
    D3DTexture* tex,
    D3D12_SRV_DIMENSION srvDimension = D3D12_SRV_DIMENSION_TEXTURE2D)
{
    LoadDDSTexture(device, commandList, filename, descriptorHeap, &tex->resource, &tex->upload, &tex->heapIndex, &tex->cpuDescriptorHandle, &tex->gpuDescriptorHandle, srvDimension);
}

struct Material {
    Material() {}

    unsigned int id;

    std::string newmtl;
    float Ns;
    DirectX::XMFLOAT3 Ka;
    DirectX::XMFLOAT3 Kd;
    DirectX::XMFLOAT3 Ks;
    DirectX::XMFLOAT3 Ke;
    float Ni;
    float d;
    float illum;
    std::string map_Bump;
    std::string map_Kd;
    std::string map_Ks;
    std::string map_Ke;
};

struct AssimpTexture {
    unsigned int id;
    std::string type;
    std::string path;
};

struct Transform {
    DirectX::XMFLOAT3 translation;
    DirectX::XMFLOAT3 scale;
    float rotation;
};

class Command;

class CommandPack {
public:
    CommandPack(Command* cmd, float sp, float time, bool strafe) : command{ cmd }, speed{ sp }, elapsedTime{ time }, strafe{ strafe } {}
    Command* command;
    float speed;
    float elapsedTime;
    bool strafe;
};