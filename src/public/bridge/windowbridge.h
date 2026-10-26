#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

bool WindowIsRunning();
uint32_t WindowGetWidth();
uint32_t WindowGetHeight();
float WindowGetAspectRatio();
int32_t WindowGetPosX();
int32_t WindowGetPosY();
bool WindowIsFullscreen();
bool WindowIsVREnabled();
void WindowGetVRPose(float* pos, float* quat);
void WindowSetVRBaseTrackingSpace(const float* pos, const float* rotQuat);

#ifdef __cplusplus
};
#endif
