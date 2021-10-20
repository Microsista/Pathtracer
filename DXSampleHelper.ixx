module;
#include <DDSTextureLoader.h>
#include <wrl.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <Windows.h>

export module DXSampleHelper;

import DescriptorHeapInterface;
import D3DTexture;

using Microsoft::WRL::ComPtr;
using namespace std;

export {
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
}

#define SAFE_RELEASE(p) if (p) (p)->Release()

export inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

export inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        OutputDebugString(msg);
        throw HrException(hr);
    }
}

export {
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

        *data = reinterpret_cast<unsigned char*>(malloc(fileInfo.EndOfFile.LowPart));
        *size = fileInfo.EndOfFile.LowPart;

        if (!ReadFile(file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
        {
            throw std::exception();
        }

        return S_OK;
    }
}


#if defined(_DEBUG) || defined(DBG)
export inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
export inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
export inline void SetName(ID3D12Object*, LPCWSTR)
{
}
export inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

export inline UINT Align(UINT size, UINT alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

export inline UINT CalculateConstantBufferByteSize(UINT byteSize)
{
    // Constant buffer size is required to be aligned.
    return Align(byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

#ifdef D3D_COMPILE_STANDARD_FILE_INCLUDE
export inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
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

export {
    template<class T>
    void ResetComPtrArray(T* comPtrArray)
    {
        for (auto& i : *comPtrArray)
        {
            i.Reset();
        }
    }

    template<class T>
    void ResetUniquePtrArray(T* uniquePtrArray)
    {
        for (auto& i : *uniquePtrArray)
        {
            i.reset();
        }
    }

    inline void CreateTextureSRV(
        ID3D12Device5* device,
        ID3D12Resource* resource,
        DescriptorHeapInterface* descriptorHeap,
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
        ID3D12GraphicsCommandList5* commandList,
        const wchar_t* filename,
        DescriptorHeapInterface* descriptorHeap,
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

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto buffer = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        // Create the GPU upload buffer.
        ThrowIfFailed(
            device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &buffer,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(ppUpload)));

        UpdateSubresources(commandList, *ppResource, *ppUpload, 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(*ppResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &transition);

        CreateTextureSRV(device, *ppResource, descriptorHeap, descriptorHeapIndex, cpuHandle, gpuHandle, srvDimension);
    }

    inline void LoadDDSTexture(
        ID3D12Device5* device,
        ID3D12GraphicsCommandList5* commandList,
        const wchar_t* filename,
        DescriptorHeapInterface* descriptorHeap,
        D3DTexture* tex,
        D3D12_SRV_DIMENSION srvDimension = D3D12_SRV_DIMENSION_TEXTURE2D)
    {
        LoadDDSTexture(device, commandList, filename, descriptorHeap, &tex->resource, &tex->upload, &tex->heapIndex, &tex->cpuDescriptorHandle, &tex->gpuDescriptorHandle, srvDimension);
    }
}