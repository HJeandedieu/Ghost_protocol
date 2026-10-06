#include "render/HealthHud.h"

#include <algorithm>
#include <cmath>

#include "entities/Player.h"
void HealthHud::advance(Meter& meter, float value, float dt) {
    if (value != meter.target) {
        if (value < meter.target) {
            meter.start = meter.shown;
            meter.elapsed = 0;
            meter.target = value;
        } else {
            meter.shown = meter.start = meter.target = value;
            meter.elapsed = kDrainSeconds;
        }
    }
    meter.elapsed = std::min(kDrainSeconds, meter.elapsed + dt);
    const float fraction = meter.elapsed / kDrainSeconds;
    meter.shown = meter.start + (meter.target - meter.start) * fraction;
}
void HealthHud::update(float dt, const Player& player) {
    if (!initialized_) {
        hp_ = {player.hp(), player.hp(), player.hp(), kDrainSeconds};
        armor_ = {player.armor(), player.armor(), player.armor(), kDrainSeconds};
        maximumHp_ = player.maximumHp();
        maximumArmor_ = player.maximumArmor();
        initialized_ = true;
    }
    if (!std::isfinite(dt) || dt < 0) return;
    flashRemaining_ = std::max(0.0f, flashRemaining_ - dt);
    advance(hp_, player.hp(), dt);
    advance(armor_, player.armor(), dt);
}
