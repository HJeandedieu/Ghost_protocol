#pragma once

#include <array>
#include <string>

class Logger;

struct PlayerConfig {
    float radius = 14.0f;
    float walk = 160.0f;
    float sprint = 260.0f;
    float crouch = 80.0f;
    float accel = 1200.0f;
    float decel = 1600.0f;
    float bagSpeedMult = 0.75f;
    float hp = 100.0f;
    float armor = 50.0f;
    float armorRegen = 8.0f;
    float armorRegenDelay = 5.0f;
};

struct PickupConfig {
    float medkitChance = 0.20f;
    float armorChance = 0.10f;
    float medkitAmount = 50;
    float armorAmount = 50;
    float collectRadius = 50;
};

struct EnemyCombatConfig {
    float reactionTime = 0.4f;
    float burstInterval = 0.12f;
    float strafeSpeed = 80;
    float strafeReverseTime = 1.5f;
    float pathRefresh = 0.5f;
    float deathFade = 0.6f;
};

struct PingConfig {
    float smallRadius = 260.0f;
    float bigRadius = 520.0f;
    float tapMax = 0.25f;
    float chargeMax = 0.8f;
    float speed = 800.0f;
    float fade = 2.5f;
    float cooldown = 3.0f;
    float noiseMult = 0.6f;
    float halo = 96.0f;
    float hazardRevealRadius = 120.0f;
};

struct RenderConfig {
    float ambientFloorAlpha = 0.18f;
    float ambientWallAlpha = 0.45f;
};

struct ViewConfig {
    float leadPx = 60.0f;
    float followRate = 8.0f;
};

struct NoiseConfig {
    float crouch = 40.0f;
    float walk = 120.0f;
    float sprint = 280.0f;
    float lockpick = 120.0f;
    float crack = 200.0f;
    float crackInterval = 5.0f;
    float shotSuppressed = 350.0f;
    float shot = 900.0f;
    float laser = 400.0f;
};

struct GuardConfig {
    float radius = 14.0f;
    float stationaryTurnSpeed = 20.0f;
    float patrolSpeed = 90.0f;
    float searchSpeed = 130.0f;
    float chaseSpeed = 200.0f;
    float coneDeg = 75.0f;
    float rangeLit = 300.0f;
    float rangeDim = 240.0f;
    float rangeDark = 180.0f;
    float crouchDarkMult = 0.7f;
    float fillFar = 35.0f;
    float fillNear = 100.0f;
    float sprintMult = 1.25f;
    float crouchMult = 0.7f;
    float decay = 25.0f;
    float callin = 3.0f;
    float takedownRange = 50.0f;
    float suspiciousTime = 1.5f;
    float searchTime = 8.0f;
    float investigateLook = 3.0f;
    float searchLoopRadius = 96.0f;
    float searchPointPause = 0.5f;
    float searchTurnRate = 90.0f;
    float lookSweepDeg = 60.0f;
    float stuckWindow = 1.0f;
    float stuckMinProgress = 8.0f;
    float arriveTolerance = 8.0f;
    float pathClearStep = 12.0f;
    float crumbSpacing = 32.0f;
    float crumbMax = 64.0f;
};

struct CameraConfig {
    float coneDeg = 60.0f;
    float range = 340.0f;
    float callin = 2.0f;
    float loopSeconds = 120.0f;
    float sweepSpeed = 20.0f;
};

struct LaserConfig {
    float touchCooldown = 1.5f;
    float secondTouchWindow = 30.0f;
};

struct PagerConfig {
    float ringDelay = 4.0f;
    float answerWindow = 12.0f;
    float answerHold = 1.5f;
};

struct AlarmConfig {
    float slowmoScale = 0.35f;
    float slowmoTime = 0.8f;
    float flipTime = 0.4f;
    float barsIn = 0.6f;
    float barsOut = 1.2f;
    float shakeTrauma = 0.8f;
    float traumaDecay = 1.5f;
    float bannerTime = 2.5f;
    float shakePixels = 12.f;
    float firstWaveDelay = 30.0f;
    float waveInterval = 25.0f;
    float maxAlive = 12.0f;
};

struct MissionRetryPosition {
    int x = 0;
    int y = 0;
};

struct MissionConfig {
    float lockpick = 4.0f;
    float securityHold = 6.0f;
    float breakerHold = 5.0f;
    float crack = 25.0f;
    float thermitePlace = 2.0f;
    float thermiteBurn = 75.0f;
    float dyeHold = 2.0f;
    float dyeBurst = 45.0f;
    float bollardHold = 4.0f;
    float vanDelay = 10.0f;
    float throwDistance = 300.0f;
    std::array<MissionRetryPosition, 4> retryPositions{{{21, 32}, {52, 16}, {52, 8}, {52, 8}}};
};

struct PayoutConfig {
    float bag = 20000.0f;
    float spoiled = 10000.0f;
    float ghostBonus = 0.25f;
    float timeBonus = 10000.0f;
    float timeBonusLimit = 600.0f;
    float handlerCut = 0.15f;
    float deathPenalty = 10000.0f;
    float rankS = 200000.0f;
    float rankA = 150000.0f;
    float rankB = 90000.0f;
};

struct DifficultyPreset {
    float enemyDmg = 1.0f;
    float detectFill = 1.0f;
    float ammo = 1.0f;
    float maxAlive = 12.0f;
};

struct DifficultyConfig {
    DifficultyPreset easy{0.6f, 0.75f, 1.5f, 12.0f};
    DifficultyPreset normal{1.0f, 1.0f, 1.0f, 12.0f};
    DifficultyPreset hard{1.4f, 1.25f, 0.8f, 16.0f};
};

class Config {
   public:
    static Config load(const std::string& path, Logger& logger);

    PlayerConfig player;
    PickupConfig pickup;
    EnemyCombatConfig enemyCombat;
    ViewConfig view;
    RenderConfig render;
    PingConfig ping;
    NoiseConfig noise;
    GuardConfig guard;
    CameraConfig camera;
    LaserConfig laser;
    PagerConfig pager;
    AlarmConfig alarm;
    MissionConfig mission;
    PayoutConfig payout;
    DifficultyConfig difficulty;
};
