#pragma once
#include <string>

struct Config {
  struct {
    struct {
      bool teammates = false;
      bool dormant = false;
      bool boundingBox = false;
      bool healthBar = false;
      bool name = false;
      bool flags = false;
      bool weaponText = false;
      bool weaponIcon = false;
      bool ammo = false;
      bool distance = false;
      bool glow = false;
      bool hitmarker = false;
      bool hitmarkerSound = false;
      bool visualizeSounds = false;
      bool lineOfSight = false;
      bool money = false;
      bool skeleton = false;
      bool outOfFOVArrow = false;
      float arrowSize = 10.0f;
      float arrowDistance = 10.0f;
    } Players;

    struct {
      bool player = false;
      bool playerBehindWall = false;
      int playerMaterial = 0;
      bool teammates = false;
      bool hands = false;
      int handsMaterial = 0;
      bool weapons = false;
      int weaponsMaterial = 0;
      bool disableModelOcclusion = false;
      bool shadow = false;
      bool localFakeShadow = false;
    } ColoredModels;

    struct {
      bool radar = false;
      int droppedWeapons = 0;
      bool droppedWeaponsAmmo = false;
      bool grenades = false;
      bool glowGrenades = false;
      bool inaccuracyOverlay = false;
      bool recoilOverlay = false;
      bool crosshair = false;
      bool bomb = false;
      bool grenadeTrajectory = false;
      bool grenadeProximityWarning = false;
      bool spectators = false;
      bool penetrationReticle = false;
      bool hostages = false;
      bool upgradeTablet = false;
      bool dangerZoneItems = false;
    } Other;

    struct {
      bool droneGun = false;
      bool blackhawk = false;
      bool drone = false;
      bool randomCase = false;
      bool toolCase = false;
      bool pistolCase = false;
      bool explosiveCase = false;
      bool heavyWeaponCase = false;
      bool dufflebag = false;
      bool jammer = false;
      bool ammoBox = false;
      bool armor = false;
      bool parachutePack = false;
      bool briefcase = false;
      bool tabletUpgradeZone = false;
      bool tabletUpgradeDrone = false;
      bool cashStack = false;
    } DangerZone;

    struct {
      bool removeFlashbangEffects = false;
      bool removeSmokeGrenades = false;
      bool removeFog = false;
      bool removeGrass = false;
      bool removeSkybox = false;
      int visualRecoilAdjustment = 0;
      float transparentWalls = 100.0f;
      float transparentProps = 100.0f;
      bool brightnessAdjustment[2]; // Changed from int for MultiCombo
      bool removeScopeOverlay = false;
      bool disablePostProcessing = false;
      bool forceThirdPerson = false;
      bool disableRenderingOfTeammates = false;
      bool bulletTracers = false;
      bool bulletImpacts = false;
    } Effects;
  } Visuals;

  struct {
    struct {
      float boundingBox[4] = {1.f, 1.f, 1.f, 1.f};
      float name[4] = {1.f, 1.f, 1.f, 1.f};
      float weaponIcon[4] = {1.f, 1.f, 1.f, 1.f};
      float ammo[4] = {1.f, 1.f, 1.f, 1.f};
      float glow[4] = {1.f, 1.f, 1.f, 1.f};
      float visualizeSounds[4] = {1.f, 1.f, 1.f, 1.f};
      float lineOfSight[4] = {1.f, 1.f, 1.f, 1.f};
      float skeleton[4] = {1.f, 1.f, 1.f, 1.f};
      float outOfFOVArrow[4] = {1.f, 1.f, 1.f, 1.f};
    } Players;

    struct {
      float player[4] = {1.f, 1.f, 1.f, 1.f};
      float playerBehindWall[4] = {1.f, 1.f, 1.f, 1.f};
      float playerReflectivityColor[4] = {1.f, 1.f, 1.f, 1.f};
      float teammates[4] = {1.f, 1.f, 1.f, 1.f};
      float hands[4] = {1.f, 1.f, 1.f, 1.f};
      float weapons[4] = {1.f, 1.f, 1.f, 1.f};
      float shadow[4] = {1.f, 1.f, 1.f, 1.f};
      float localFakeShadow[4] = {1.f, 1.f, 1.f, 1.f};
    } ColoredModels;

    struct {
      float glowGrenades[4] = {1.f, 1.f, 1.f, 1.f};
      float grenades[4] = {1.f, 1.f, 1.f, 1.f};
      float inaccuracyOverlay[4] = {1.f, 1.f, 1.f, 1.f};
      float bomb[4] = {1.f, 1.f, 1.f, 1.f};
      float grenadeTrajectory[4] = {1.f, 1.f, 1.f, 1.f};
    } Other;
  } Color;

  struct {
    float overrideFov = 90.0f;
    bool bunnyHop = false;
    bool airStrafe = false;
    int airStrafeType = 0;
    bool knifeBot = false;
    bool knifeBotSettings[2];
    bool zeusBot = false;
    bool blockBot = false;
    bool automaticWeapons = false;
    bool jumpAtEdge = false;
    bool ragdollForce = false;
    bool ragdollGravity = false;
    bool revealCompetitiveRanks = false;
    bool autoAcceptMatchmaking = false;
    bool clantagSpammer = false;
    bool logWeaponPurchases = false;
    bool logDamageDealt = false;
    bool fastWalk = false;
    bool freeLook = false;
    bool persistentKillfeed = false;
    bool antiUntrusted = true;
    bool antiScreenshot = false;
    bool lowFpsWarning = false;
  } Misc;
};

extern Config g_Config;
extern bool unload;

// Stub arrays for combos
