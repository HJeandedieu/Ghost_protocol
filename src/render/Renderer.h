#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Config.h"
#include "core/Vec2.h"
#include "entities/RecoveryPickup.h"
#include "raylib.h"
#include "render/HealthHud.h"

struct Level;
class Player;
class Guard;
class SecurityCamera;
class Laser;
class RippleSystem;
class Logger;
class InteractionSystem;
class TileMap;
class AlarmDirector;
class CombatSystem;
class EnemyCombatSystem;
class Enemy;
class WaveSpawner;
class AlarmSequence;
class PagerSystem;
class ObjectiveSystem;
struct World;

class Renderer {
   public:
    explicit Renderer(Logger& logger, const RenderConfig& config = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void beginFrame();
    void present();
    void setReduceEffects(bool enabled) { reduceEffects_ = enabled; }
    Texture2D frameTexture() const { return surface_.texture; }
    void prepareLevel(const Level& level);
    void drawInteractionHud(const World& world, const InteractionSystem& interaction,
                            float noiseRadius, float maximumNoise,
                            const ObjectiveSystem* objectives = nullptr) const;
    void drawBusted(int stage) const;
    static void drawPlaceholder(const char* title, const char* subtitle);
    static void drawError(const char* message);
    void resetHealthHud(const Player& player) {
        healthHud_ = HealthHud{};
        healthHud_.update(0, player);
    }
    void notifyHealthDamage() { healthHud_.damaged(); }
    void updateHealthHud(float dt, const Player& player) { healthHud_.update(dt, player); }
    void drawWeaponHud(const CombatSystem& combat) const;
    void drawWaveHud(const WaveSpawner& waves) const;
    void drawAlarmSequence(const AlarmSequence& sequence) const;
    void drawPickupHud(const RecoveryPickup* pickup) const;
    void drawStealthHud(const AlarmDirector& alarm, const PagerSystem& pagers,
                        const std::vector<Guard>& guards) const;
    void drawLevel(const Level& level, const Player& player, const RippleSystem& ripple,
                   Vec2 cameraTarget, float facing, float alpha, bool overview, std::uint32_t seed,
                   const std::vector<Guard>& guards = {},
                   const std::vector<SecurityCamera>& cameras = {},
                   const std::vector<Laser>& lasers = {}, bool securityLooped = false,
                   const CombatSystem* combat = nullptr,
                   const std::vector<std::unique_ptr<RecoveryPickup>>& pickups = {},
                   bool pickupsLit = false, const std::vector<std::unique_ptr<Enemy>>& enemies = {},
                   const EnemyCombatSystem* enemyCombat = nullptr,
                   const AlarmSequence* alarmSequence = nullptr,
                   const ObjectiveSystem* objectives = nullptr);

   private:
    RenderTexture2D surface_;
    RenderTexture2D world_;
    RenderConfig config_;
    HealthHud healthHud_;
    Shader post_{};
    int timeLocation_ = -1;
    int alarmPulseLocation_ = -1;
    float alarmPulse_ = 0;
    bool composed_ = false;
    bool reduceEffects_ = false;
    std::vector<float> coneAngles_;
    std::vector<float> coneDistances_;
    std::vector<Vec2> coneDirections_;
    struct ConeLightRegion {
        Rectangle bounds;
        float range;
    };
    std::vector<ConeLightRegion> coneLightRegions_;
    void drawGuardCone(const Guard& guard, Vec2 position, const TileMap& map, bool crouched,
                       float visibility);
    void compose(bool effects);
};
