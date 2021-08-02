#include "stdafx.h"
#include "DXCore.h"
#include "Light.h"

using namespace Microsoft::WRL;
using namespace std;

DXCore::DXCore(UINT width, UINT height, std::wstring name) :
    m_width(width),
    m_height(height),
    m_windowBounds{ 0,0,0,0 },
    m_title(name),
    m_aspectRatio(0.0f),
    m_enableUI(true),
    m_adapterIDoverride(UINT_MAX),
    m_camera(new Camera),
    m_light(new Light)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    UpdateForSizeChange(width, height);
}

DXCore::~DXCore()
{
}

void DXCore::UpdateForSizeChange(UINT clientWidth, UINT clientHeight)
{
    m_width = clientWidth;
    m_height = clientHeight;
    m_aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
    m_camera->SetLens(0.25f * DirectX::XM_PI, m_aspectRatio, 1.0f, 1000.0f);
}

// Helper function for resolving the full path of assets.
std::wstring DXCore::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}


// Helper function for setting the window's title text.
void DXCore::SetCustomWindowText(LPCWSTR text)
{
    std::wstring windowText = m_title + L": " + text;
    SetWindowText(Win32Core::GetHwnd(), windowText.c_str());
}

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_
void DXCore::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        // -disableUI
        if (_wcsnicmp(argv[i], L"-disableUI", wcslen(argv[i])) == 0 ||
            _wcsnicmp(argv[i], L"/disableUI", wcslen(argv[i])) == 0)
        {
            m_enableUI = false;
        }
        // -forceAdapter [id]
        else if (_wcsnicmp(argv[i], L"-forceAdapter", wcslen(argv[i])) == 0 ||
            _wcsnicmp(argv[i], L"/forceAdapter", wcslen(argv[i])) == 0)
        {
            ThrowIfFalse(i + 1 < argc, L"Incorrect argument format passed in.");
            
            m_adapterIDoverride = _wtoi(argv[i + 1]);
            i++;
        }
    }

}

void DXCore::SetWindowBounds(int left, int top, int right, int bottom)
{
    m_windowBounds.left = static_cast<LONG>(left);
    m_windowBounds.top = static_cast<LONG>(top);
    m_windowBounds.right = static_cast<LONG>(right);
    m_windowBounds.bottom = static_cast<LONG>(bottom);
}