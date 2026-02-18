#pragma once
#include <chrono>

// Calculate smooth aim movement towards target
// targetX, targetY: relative position from crosshair center
// outMoveX, outMoveY: calculated mouse movement
// deltaTime: time since last frame in seconds
void calculateAimMove(float targetX, float targetY, int &outMoveX,
                      int &outMoveY, float deltaTime, bool isShooting);

// Reset aiming state (velocity, accumulation) when target changes
void resetAimingState();

// Apply RCS (Recoil Control System) adjustments
void applyRCS(int &moveX, int &moveY, float deltaTime, bool isShootingStarted,
              std::chrono::steady_clock::time_point shootStartTime);