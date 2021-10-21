module;
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <vector>
export module PerformanceComponent;

import DeviceResources;
import StepTimer;
import PerformanceTimers;
import RaytracingSceneDefines;
import Helper;

using namespace std;

export class PerformanceComponent {
    DeviceResources* deviceResources;
    UINT width;
    UINT height;
    StepTimer* timer;
    vector<DX::GPUTimer>* gpuTimers;

public:
    PerformanceComponent() {}

    void CalculateFrameStats() {
        static int frameCnt = 0;
        static double prevTime = 0.0f;
        double totalTime = timer->GetTotalSeconds();

        frameCnt++;

        // Compute averages over one second period.
        if ((totalTime - prevTime) >= 1.0f)
        {
            float diff = static_cast<float>(totalTime - prevTime);
            float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

            frameCnt = 0;
            prevTime = totalTime;
            float raytracingTime = static_cast<float>((*gpuTimers)[GpuTimers::Raytracing].GetElapsedMS());
            float MRaysPerSecond = NumMRaysPerSecond(width, height, raytracingTime);

            wstringstream windowText;
            windowText << setprecision(2) << fixed
                << L"    FPS: " << fps
                << L"    Raytracing time: " << raytracingTime << " ms"
                << L"    Ray throughput: " << MRaysPerSecond << " MRPS"
                << L"    GPU[" << deviceResources->GetAdapterID() << L"]: " << deviceResources->GetAdapterDescription();
            /*SetCustomWindowText(windowText.str().c_str(), hwnd);*/
        }
    }
};