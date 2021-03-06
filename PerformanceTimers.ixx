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


#ifndef IID_GRAPHICS_PPV_ARGS
#define IID_GRAPHICS_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif

#include <exception>
#include <stdexcept>

export module PerformanceTimers;

import DXSampleHelper;

using namespace DirectX;
using namespace DX;
using Microsoft::WRL::ComPtr;

namespace
{
    inline float lerp(float a, float b, float f)
    {
        return (1.f - f) * a + f * b;
    }

    inline float UpdateRunningAverage(float avg, float value)
    {
        return lerp(value, avg, 0.95f);
    }

    inline void DebugWarnings(uint32_t timerid, uint64_t start, uint64_t end)
    {
#if defined(_DEBUG)
        if (!start && end > 0)
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: Timer %u stopped but not started\n", timerid);
            OutputDebugStringA(buff);
        }
        else if (start > 0 && !end)
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: Timer %u started but not stopped\n", timerid);
            OutputDebugStringA(buff);
        }
#else
        UNREFERENCED_PARAMETER(timerid);
        UNREFERENCED_PARAMETER(start);
        UNREFERENCED_PARAMETER(end);
#endif
    }
};

export namespace DX
{

    class CPUTimer
    {
    public:
        static const size_t c_maxTimers = 8;

        CPUTimer() :
            m_cpuFreqInv(1.f),
            m_start{},
            m_end{},
            m_avg{}
        {
            LARGE_INTEGER cpuFreq;
            if (!QueryPerformanceFrequency(&cpuFreq))
            {
                throw std::exception("QueryPerformanceFrequency");
            }

            m_cpuFreqInv = 1000.0 / double(cpuFreq.QuadPart);
        }

        CPUTimer(const CPUTimer&) = delete;
        CPUTimer& operator=(const CPUTimer&) = delete;

        CPUTimer(CPUTimer&&) = default;
        CPUTimer& operator=(CPUTimer&&) = default;

        // Start/stop a particular performance timer (don't start same index more than once in a single frame)
        void Start(uint32_t timerid = 0)
        {
            if (timerid >= c_maxTimers)
                throw std::out_of_range("Timer ID out of range");

            if (!QueryPerformanceCounter(&m_start[timerid]))
            {
                throw std::exception("QueryPerformanceCounter");
            }
        }
        void Stop(uint32_t timerid = 0)
        {
            if (timerid >= c_maxTimers)
                throw std::out_of_range("Timer ID out of range");

            if (!QueryPerformanceCounter(&m_end[timerid]))
            {
                throw std::exception("QueryPerformanceCounter");
            }
        }

        // Should Update once per frame to compute timer results
        void Update()
        {
            for (uint32_t j = 0; j < c_maxTimers; ++j)
            {
                uint64_t start = m_start[j].QuadPart;
                uint64_t end = m_end[j].QuadPart;

                DebugWarnings(j, start, end);

                float value = float(double(end - start) * m_cpuFreqInv);
                m_avg[j] = UpdateRunningAverage(m_avg[j], value);
            }
        }

        // Reset running average
        void Reset()
        {
            memset(m_avg, 0, sizeof(m_avg));
        }

        // Returns delta time in milliseconds
        double GetElapsedMS(uint32_t timerid = 0) const
        {
            if (timerid >= c_maxTimers)
                return 0.0;

            uint64_t start = m_start[timerid].QuadPart;
            uint64_t end = m_end[timerid].QuadPart;

            return double(end - start) * m_cpuFreqInv;
        }

        // Returns running average in milliseconds
        float GetAverageMS(uint32_t timerid = 0) const
        {
            return (timerid < c_maxTimers) ? m_avg[timerid] : 0.f;
        }

    private:
        double          m_cpuFreqInv;
        LARGE_INTEGER   m_start[c_maxTimers];
        LARGE_INTEGER   m_end[c_maxTimers];
        float           m_avg[c_maxTimers];
    };


    //----------------------------------------------------------------------------------
    // DirectX 12 implementation of GPU timer
    class GPUTimer
    {
    public:
        static const size_t c_maxTimers = 8;

        GPUTimer() :
            m_gpuFreqInv(1.f),
            m_avg{},
            m_timing{},
            m_maxframeCount(0)
        {}

        GPUTimer(ID3D12Device* device, ID3D12CommandQueue* commandQueue, UINT maxFrameCount, _In_ ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator) :
            m_gpuFreqInv(1.f),
            m_avg{},
            m_timing{}
        {
            RestoreDevice(device, commandQueue, maxFrameCount, commandList, commandAllocator);
        }

        GPUTimer(const GPUTimer&) = delete;
        GPUTimer& operator=(const GPUTimer&) = delete;

        GPUTimer(GPUTimer&&) = default;
        GPUTimer& operator=(GPUTimer&&) = default;

        ~GPUTimer() { ReleaseDevice(); }

        // Indicate beginning & end of frame
        void BeginFrame(_In_ ID3D12GraphicsCommandList* commandList)
        {
            UNREFERENCED_PARAMETER(commandList);
            //commandList->BeginQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
        }
        void EndFrame(_In_ ID3D12GraphicsCommandList* commandList)
        {
            // Resolve query for the current frame.
            static UINT resolveToFrameID = 0;
            UINT64 resolveToBaseAddress = resolveToFrameID * c_timerSlots * sizeof(UINT64);
            commandList->ResolveQueryData(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, c_timerSlots, m_buffer.Get(), resolveToBaseAddress);

            // Grab read-back data for the queries from a finished frame m_maxframeCount ago.                                                           
            UINT readBackFrameID = (resolveToFrameID + 1) % (m_maxframeCount + 1);
            SIZE_T readBackBaseOffset = readBackFrameID * c_timerSlots * sizeof(UINT64);
            D3D12_RANGE dataRange =
            {
                readBackBaseOffset,
                readBackBaseOffset + c_timerSlots * sizeof(UINT64),
            };

            UINT64* timingData;
            ThrowIfFailed(m_buffer->Map(0, &dataRange, reinterpret_cast<void**>(&timingData)));
            memcpy(m_timing, timingData, sizeof(UINT64) * c_timerSlots);
            m_buffer->Unmap(0, nullptr);

            for (uint32_t j = 0; j < c_maxTimers; ++j)
            {
                UINT64 start = m_timing[j * 2];
                UINT64 end = m_timing[j * 2 + 1];

                DebugWarnings(j, start, end);

                float value = float(double(end - start) * m_gpuFreqInv);
                m_avg[j] = UpdateRunningAverage(m_avg[j], value);
            }

            resolveToFrameID = readBackFrameID;
        }

        // Start/stop a particular performance timer (don't start same index more than once in a single frame)
        void Start(_In_ ID3D12GraphicsCommandList* commandList, uint32_t timerid = 0)
        {
            if (timerid >= c_maxTimers)
                throw std::out_of_range("Timer ID out of range");

            commandList->EndQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timerid * 2);
        }
        void Stop(_In_ ID3D12GraphicsCommandList* commandList, uint32_t timerid = 0)
        {
            if (timerid >= c_maxTimers)
                throw std::out_of_range("Timer ID out of range");

            commandList->EndQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timerid * 2 + 1);
        }

        // Reset running average
        void Reset()
        {
            memset(m_avg, 0, sizeof(m_avg));
        }

        // Returns delta time in milliseconds
        double GetElapsedMS(uint32_t timerid = 0) const
        {
            if (timerid >= c_maxTimers)
                return 0.0;

            UINT64 start = m_timing[timerid * 2];
            UINT64 end = m_timing[timerid * 2 + 1];

            if (end < start)
                return 0.0;

            return double(end - start) * m_gpuFreqInv;
        }

        // Returns running average in milliseconds
        float GetAverageMS(uint32_t timerid = 0) const
        {
            return (timerid < c_maxTimers) ? m_avg[timerid] : 0.f;
        }

        // Device management
        void ReleaseDevice()
        {
            m_heap.Reset();
            m_buffer.Reset();
        }

        void RestoreDevice(_In_ ID3D12Device* device, _In_ ID3D12CommandQueue* commandQueue, UINT maxFrameCount, _In_ ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator)
        {
            assert(device != 0 && commandQueue != 0);
            m_maxframeCount = maxFrameCount;

            // Filter a debug warning coming when accessing a readback resource for the timing queries.
            // The readback resource handles multiple frames data via per-frame offsets within the same resource and CPU
            // maps an offset written "frame_count" frames ago and the data is guaranteed to had been written to by GPU by this time. 
            // Therefore the race condition doesn't apply in this case.
            ComPtr<ID3D12InfoQueue> d3dInfoQueue;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&d3dInfoQueue))))
            {
                // Suppress individual messages by their ID.
                D3D12_MESSAGE_ID denyIds[] =
                {
                    D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumIDs = _countof(denyIds);
                filter.DenyList.pIDList = denyIds;
                d3dInfoQueue->AddStorageFilterEntries(&filter);
                OutputDebugString(L"Warning: GPUTimer is disabling an unwanted D3D12 debug layer warning: D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED.");
            }


            UINT64 gpuFreq;
            ThrowIfFailed(commandQueue->GetTimestampFrequency(&gpuFreq));
            m_gpuFreqInv = 1000.0 / double(gpuFreq);

            D3D12_QUERY_HEAP_DESC desc = {};
            desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            desc.Count = c_timerSlots;
            ThrowIfFailed(device->CreateQueryHeap(&desc, IID_GRAPHICS_PPV_ARGS(m_heap.ReleaseAndGetAddressOf())));
            m_heap->SetName(L"GPUTimerHeap");
             // Reset the command list for the acceleration structure construction.
            commandList->Reset(commandAllocator, nullptr);

            for (int i = 0; i < c_timerSlots; i++)
                commandList->EndQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, i);

            auto readBack = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

            // We allocate m_maxframeCount + 1 instances as an instance is guaranteed to be written to if maxPresentFrameCount frames
            // have been submitted since. This is due to a fact that Present stalls when none of the m_maxframeCount frames are done/available.
            size_t nPerFrameInstances = m_maxframeCount + 1;

            auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(nPerFrameInstances * c_timerSlots * sizeof(UINT64));
            ThrowIfFailed(device->CreateCommittedResource(
                &readBack,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_buffer.ReleaseAndGetAddressOf()))
            );
            m_buffer->SetName(L"GPUTimerBuffer");
        }

    private:
        static const size_t c_timerSlots = c_maxTimers * 2;

        Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_heap;
        Microsoft::WRL::ComPtr<ID3D12Resource>  m_buffer;
        double                                  m_gpuFreqInv;
        float                                   m_avg[c_maxTimers];
        UINT64                                  m_timing[c_timerSlots];
        size_t                                  m_maxframeCount;

    };
}