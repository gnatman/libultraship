#pragma once

#ifdef _WIN32

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <string>
#include <SDL2/SDL.h>

namespace Ship {

class DirectInputFFBDevice {
  public:
    DirectInputFFBDevice(SDL_JoystickGUID guid, const std::string& name);
    ~DirectInputFFBDevice();

    bool Init();
    void SetConstantForce(float magnitude);
    void SetSpring(float coefficient, float offset);
    void Release();

  private:
    static BOOL CALLBACK EnumDevicesCallback(const DIDEVICEINSTANCE* pdidi, VOID* pContext);
    static BOOL CALLBACK EnumEffectsCallback(LPCDIEFFECTINFO pdei, LPVOID pvRef);

    bool CreateConstantForceEffect();
    bool CreateSpringEffect();

    SDL_JoystickGUID mSDLGuid;
    std::string mName;
    GUID mDInputGuid;
    bool mGuidFound = false;

    LPDIRECTINPUT8 mDInput = nullptr;
    LPDIRECTINPUTDEVICE8 mDevice = nullptr;
    LPDIRECTINPUTEFFECT mConstantForceEffect = nullptr;
    LPDIRECTINPUTEFFECT mSpringEffect = nullptr;

    bool mIsAcquired = false;
};

} // namespace Ship

#endif
