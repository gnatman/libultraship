#ifdef _WIN32
#include "DirectInputFFBDevice.h"
#include <spdlog/spdlog.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace Ship {

DirectInputFFBDevice::DirectInputFFBDevice(SDL_JoystickGUID guid, const std::string& name)
    : mSDLGuid(guid), mName(name) {
}

DirectInputFFBDevice::~DirectInputFFBDevice() {
    Release();
}

BOOL CALLBACK DirectInputFFBDevice::EnumDevicesCallback(const DIDEVICEINSTANCE* pdidi, VOID* pContext) {
    DirectInputFFBDevice* self = static_cast<DirectInputFFBDevice*>(pContext);
    
    // In a real implementation, we would compare the DInput GUID with the SDL GUID.
    // For now, we'll assume the first game controller we find that matches the name or is a wheel.
    self->mDInputGuid = pdidi->guidInstance;
    self->mGuidFound = true;
    return DIENUM_STOP;
}

bool DirectInputFFBDevice::Init() {
    HRESULT hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&mDInput, NULL);
    if (FAILED(hr)) return false;

    hr = mDInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, this, DIEDFL_ATTACHEDONLY);
    if (FAILED(hr) || !mGuidFound) {
        SPDLOG_ERROR("DirectInput8 failed to find device for {}", mName);
        return false;
    }

    hr = mDInput->CreateDevice(mDInputGuid, &mDevice, NULL);
    if (FAILED(hr)) {
        SPDLOG_ERROR("DirectInput8 failed to create device for {}", mName);
        return false;
    }

    hr = mDevice->SetDataFormat(&c_dfDIJoystick);
    if (FAILED(hr)) return false;

    // Get HWND from SDL
    HWND hwnd = NULL;
    SDL_Window* sdlWnd = SDL_GL_GetCurrentWindow();
    if (sdlWnd) {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(sdlWnd, &wmInfo)) {
            hwnd = wmInfo.info.win.window;
        }
    }

    if (!hwnd) {
        SPDLOG_ERROR("DirectInput8 failed to get HWND for {}", mName);
        return false;
    }

    hr = mDevice->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    if (FAILED(hr)) {
        SPDLOG_ERROR("DirectInput8 failed to set cooperative level for {}", mName);
        return false;
    }

    // Disable autocenter
    DIPROPDWORD dipdw;
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAUTOCENTER_OFF;
    mDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph);

    hr = mDevice->Acquire();
    if (FAILED(hr)) {
        SPDLOG_ERROR("DirectInput8 failed to acquire device for {}", mName);
        return false;
    }
    mIsAcquired = true;

    CreateConstantForceEffect();
    CreateSpringEffect();

    SPDLOG_INFO("DirectInput8 FFB Device Initialized and Acquired for {}", mName);
    return true;
}

bool DirectInputFFBDevice::CreateConstantForceEffect() {
    DWORD axes[1] = { DIJOFS_X };
    LONG direction[1] = { 0 };
    DICONSTANTFORCE diConstant;
    diConstant.lMagnitude = 0;

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain = 10000;
    eff.cAxes = 1;
    eff.rgdwAxes = axes;
    eff.rglDirection = direction;
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &diConstant;

    HRESULT hr = mDevice->CreateEffect(GUID_ConstantForce, &eff, &mConstantForceEffect, NULL);
    if (SUCCEEDED(hr)) {
        mConstantForceEffect->Start(1, 0);
        return true;
    }
    return false;
}

bool DirectInputFFBDevice::CreateSpringEffect() {
    DWORD axes[1] = { DIJOFS_X };
    LONG direction[1] = { 0 };
    DICONDITION diCondition;
    diCondition.lOffset = 0;
    diCondition.lPositiveCoefficient = 10000;
    diCondition.lNegativeCoefficient = 10000;
    diCondition.dwPositiveSaturation = 10000;
    diCondition.dwNegativeSaturation = 10000;
    diCondition.lDeadBand = 0;

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain = 10000;
    eff.cAxes = 1;
    eff.rgdwAxes = axes;
    eff.rglDirection = direction;
    eff.cbTypeSpecificParams = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &diCondition;

    HRESULT hr = mDevice->CreateEffect(GUID_Spring, &eff, &mSpringEffect, NULL);
    if (SUCCEEDED(hr)) {
        mSpringEffect->Start(1, 0);
        return true;
    }
    return false;
}

void DirectInputFFBDevice::SetConstantForce(float magnitude) {
    if (!mConstantForceEffect) return;
    
    DICONSTANTFORCE diConstant;
    diConstant.lMagnitude = (LONG)(magnitude * 10000.0f);

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &diConstant;

    mConstantForceEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void DirectInputFFBDevice::SetSpring(float coefficient, float offset) {
    if (!mSpringEffect) return;

    DICONDITION diCondition;
    diCondition.lOffset = (LONG)(offset * 10000.0f);
    diCondition.lPositiveCoefficient = (LONG)(coefficient * 10000.0f);
    diCondition.lNegativeCoefficient = (LONG)(coefficient * 10000.0f);
    diCondition.dwPositiveSaturation = 10000;
    diCondition.dwNegativeSaturation = 10000;
    diCondition.lDeadBand = 0;

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.cbTypeSpecificParams = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &diCondition;

    mSpringEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void DirectInputFFBDevice::Release() {
    if (mConstantForceEffect) { mConstantForceEffect->Release(); mConstantForceEffect = nullptr; }
    if (mSpringEffect) { mSpringEffect->Release(); mSpringEffect = nullptr; }
    if (mDevice) {
        mDevice->Unacquire();
        mDevice->Release();
        mDevice = nullptr;
    }
    if (mDInput) { mDInput->Release(); mDInput = nullptr; }
}

} // namespace Ship
#endif
