#include "aiming.hpp"
#include "config.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

static float velocityX = 0.0f, velocityY = 0.0f;
static float jitterAccumX = 0.0f, jitterAccumY = 0.0f;
static float g_bezierSeed = 1.0f;

// Phase 16: Sticky Center State
static float g_lockTargetX = 0.0f, g_lockTargetY = 0.0f;
static bool g_isLocked = false;
static auto g_lastOffTargetTime = std::chrono::steady_clock::now();

static std::default_random_engine rng(std::random_device{}());

// Clamp helper
inline int clampVal(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void calculateAimMove(float targetX, float targetY, int &outMoveX,
                      int &outMoveY, float deltaTime, bool isShooting) {
  float dist = std::sqrt(targetX * targetX + targetY * targetY);

  // 1. Micro-Precision Deadzone
  // Phase 34: Minimal threshold to prevent HID jitter while stationary.
  if (dist < 0.5f) {
    velocityX = 0;
    velocityY = 0;
    outMoveX = 0;
    outMoveY = 0;
    return;
  }

  // 2. Control Logic
  // Humanization: Subtle LFO / Noise
  float noiseX = 0.0f, noiseY = 0.0f;
  if (config.noiseAmplification > 0.01f) {
    static float timeAcc = 0.0f;
    timeAcc += deltaTime * config.noiseFrequency;
    noiseX = std::sin(timeAcc) * config.noiseAmplification;
    noiseY = std::cos(timeAcc * 1.3f) * config.noiseAmplification;
  }

  // Bezier curve (Aggressive Organic Pathing)
  // Bezier curve (Aggressive Organic Pathing)
  float curveX = 0.0f, curveY = 0.0f;
  if (config.bezierStrength > 0.01f) {
    float perpX = -targetY / dist;
    float perpY = targetX / dist;

    // Phase 36: Bezier Ease-Out
    // Smoothly fade the curve magnitude as we approach the target to prevent
    // "hooks"
    float curveFade = std::max(0.0f, std::min(1.0f, dist / 20.0f));

    // Magnitude scales with distance but fades to 0 at center
    float curveMag = config.bezierStrength * (dist / 10.0f) * g_bezierSeed *
                     2.0f * curveFade;

    curveX = perpX * curveMag;
    curveY = perpY * curveMag;
  }

  // Snappy/Laser Mode (Direct Control Bypass) - Restored for speed
  if (config.aimSmoothing < 1.5f) {
    // Redo Phase 18: Instantaneous gain for max speed
    float snapGain = config.speed * 15.0f;
    // We include humanization (curve/noise) even in fast mode!
    velocityX = (targetX + curveX + noiseX) * snapGain;
    velocityY = (targetY + curveY + noiseY) * snapGain;
  } else {
    // Physics Logic (Smooth Mode)
    float t =
        std::max(0.0f, std::min(1.0f, dist / config.distanceBlendThreshold));
    // Phase 35: Recoil Stability & Torque Boost
    float currentAgility = config.agilityFactor;
    if (isShooting) {
      currentAgility *= 0.5f; // Dampen agility by 50% when shooting
    }

    // Phase 34: Torque Normalization (High-Hz Floor)
    // Ensures we never lose tracking "juice" at close range.
    // Boosted slightly more (2.5 -> 3.5) for Phase 35.
    float speedWeight =
        (config.trackSpeed + (config.snapSpeed - config.trackSpeed) * t) *
        3.5f * currentAgility;

    float targetVelX = (targetX + curveX + noiseX) * config.speed * speedWeight;
    float targetVelY = (targetY + curveY + noiseY) * config.speed * speedWeight;

    // Phase 32: High-Gain Acceleration (Proportional Term)
    float smoothingFactor = std::max(1.0f, config.aimSmoothing);
    float lambda =
        (config.accelNormal * 1500.0f * currentAgility) / smoothingFactor;
    float alpha =
        std::max(0.0f, std::min(1.0f, 1.0f - std::exp(-lambda * deltaTime)));

    // Apply P-Term (Proportional Shift)
    velocityX += (targetVelX - velocityX) * alpha;
    velocityY += (targetVelY - velocityY) * alpha;

    // Phase 33: Kinetic Glue & Directional Damping
    // Only apply braking if we are actually moving towards the target center.
    // If the target is moving away (falling behind), we need zero drag.
    float dotProduct = (velocityX * targetX) + (velocityY * targetY);
    bool isMovingTowards = dotProduct > 0.0f;

    // Phase 32: Derivative Braking (D-Term)
    if (dist < 50.0f && isMovingTowards) {
      // D-Gain scales more aggressively to catch 3.5x+ momentum
      float scaleMultiplier = std::max(1.0f, config.speedScaling / 1.0f);
      float dGain =
          0.007f * scaleMultiplier * config.accelBrake * currentAgility;
      velocityX -= (velocityX * dGain);
      velocityY -= (velocityY * dGain);
    }

    // Phase 32: Agility-Compensated Friction / Dynamic Damping
    float adaptiveFriction = config.friction / std::max(0.1f, currentAgility);
    adaptiveFriction *= (1.0f + (config.speedScaling - 1.0f) * 0.2f);

    // KINETIC FIX: slash friction if falling behind or target is escaped
    if (!isMovingTowards) {
      adaptiveFriction *= 0.9f; // "Glue" mode: zero resistance
    }

    // Phase 36: Terminal Damping (Critical Braking)
    // If we are VERY close (arrival), we need to kill energy to prevent bounce.
    if (dist < 5.0f) {
      // "Soft Landing": Triple the braking force
      float arrivalDamping = (1.0f - (dist / 5.0f)) * config.accelBrake * 3.0f;
      adaptiveFriction *= (1.0f + arrivalDamping);

      // Force decay on velocity to prevent micro-orbiting
      velocityX *= 0.6f;
      velocityY *= 0.6f;
    }

    float frictionFactor = 1.0f - (adaptiveFriction * deltaTime * 5.0f);
    velocityX *= std::max(0.01f, frictionFactor);
    velocityY *= std::max(0.01f, frictionFactor);
  }

  // 3. Hard Axis Clamp (Overshoot Prevention)
  // Phase 21: Predictive clamp gets tighter at high speeds
  float clampThreshold = 0.1f / std::max(1.0f, config.speedScaling);
  if ((targetX > 0 && velocityX < -clampThreshold) ||
      (targetX < 0 && velocityX > clampThreshold))
    velocityX = 0;
  if ((targetY > 0 && velocityY < -clampThreshold) ||
      (targetY < 0 && velocityY > clampThreshold))
    velocityY = 0;

  // 4. Sub-pixel accumulation
  jitterAccumX += velocityX * config.speedScaling * deltaTime;
  jitterAccumY += velocityY * config.speedScaling * deltaTime;

  int iX = (int)jitterAccumX;
  int iY = (int)jitterAccumY;

  jitterAccumX -= iX;
  jitterAccumY -= iY;

  // 5. Hard Step Logic (Precision)
  // Phase 14: Ensure the bot moves at least 1 pixel if outside the center
  outMoveX = clampVal(iX, -(int)config.maxStep, (int)config.maxStep);
  outMoveY = clampVal(iY, -(int)config.maxStep, (int)config.maxStep);

  // REDO Phase 18.2: Removed Step Gating. It causes "diagonal vibration" at
  // 1000Hz by forcing 1-pixel steps too fast. Let accumulation handle it.
  if (dist < 1.5f) {
    g_lastOffTargetTime = std::chrono::steady_clock::now();
  }
}

// Reset aiming state (call when target changes or lost)
void resetAimingState() {
  velocityX = 0;
  velocityY = 0;
  jitterAccumX = 0;
  jitterAccumY = 0;

  // Randomize Bezier seed for next target
  std::uniform_real_distribution<float> distVal(0.0f, 1.0f);
  std::uniform_int_distribution<int> distSign(0, 1);
  float randomVal = distVal(rng);
  float sign = (distSign(rng) == 0) ? 1.0f : -1.0f;
  g_bezierSeed = sign * (0.5f + randomVal); // Result: +/- [0.5 ... 1.5]
}

// Apply RCS (Recoil Control System) pull
void applyRCS(int &moveX, int &moveY, float deltaTime, bool isShootingStarted,
              std::chrono::steady_clock::time_point shootStartTime) {
  if (!config.rcsEnabled)
    return;

  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - shootStartTime)
                .count();

  // Phase 37: Extended RCS (1400ms + Sustained Comp)
  // User requested "Double it to 1400".
  if (ms >= config.rcsDelay) {
    // 0.0 -> 1.0 over 1400ms
    float progress = (float)(ms - config.rcsDelay) / 1400.0f;

    // Clamp at 1.0 for steady-state pull (Sustained Fire Support)
    // This fixed the "pulls down then stops" issue.
    if (progress > 1.0f)
      progress = 1.0f;

    // Stop RCS after a very long time (e.g., 3000ms) to reset?
    // Or just let it pull until mouse up. HID link handles mouse up reset.
    // User requested 1400ms limit, but clamping makes more sense for
    // "sustained". I will respect the soft limit of the curve but apply max
    // pull thereafter.

    float rY_pull =
        (config.rcsStartSpeed - (progress * config.rcsProgressSpeed)) *
        config.rcsMainSpeed * config.rcsStrengthY * 100.0f;

    // Stable RCS: Direct pull based on StrengthX (drift) rather than
    // oscillation
    float rX_pull = config.rcsStrengthX * 10.0f;

    moveX += (int)(rX_pull * deltaTime);
    moveY += (int)(rY_pull * deltaTime);
  }
}
