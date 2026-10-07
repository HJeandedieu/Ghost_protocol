#include "core/Config.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

#include "core/Logger.h"

namespace {
float readNumber(const nlohmann::json& group, const char* key, float fallback,
                 const std::string& path, Logger& logger,
                 float maximum = std::numeric_limits<float>::max()) {
    const auto entry = group.is_object() ? group.find(key) : group.end();
    if (entry != group.end() && entry->is_number()) {
        const double value = entry->get<double>();
        if (std::isfinite(value) && value >= 0.0 && value <= maximum) {
            return static_cast<float>(value);
        }
    }
    logger.log(LogLevel::Warn,
               "Missing or invalid tuning key: " + path + "." + key + "; using default");
    return fallback;
}

const nlohmann::json& groupOrEmpty(const nlohmann::json& parent, const char* key,
                                   const nlohmann::json& empty) {
    const auto entry = parent.is_object() ? parent.find(key) : parent.end();
    return entry != parent.end() && entry->is_object() ? *entry : empty;
}
}  // namespace

Config Config::load(const std::string& path, Logger& logger) {
    Config config;
    std::ifstream file(path);
    if (!file) {
        logger.log(LogLevel::Warn, "Cannot open tuning file: " + path + "; using defaults");
        return config;
    }
    nlohmann::json data;
    try {
        file >> data;
    } catch (const nlohmann::json::exception& error) {
        logger.log(LogLevel::Warn, "Invalid tuning file: " + path + ": " + error.what());
        return config;
    }
    const nlohmann::json empty = nlohmann::json::object();
    const auto& player = groupOrEmpty(data, "player", empty);
    config.player.radius = readNumber(player, "radius", config.player.radius, "player", logger);
    config.player.walk = readNumber(player, "walk", config.player.walk, "player", logger);
    config.player.sprint = readNumber(player, "sprint", config.player.sprint, "player", logger);
    config.player.crouch = readNumber(player, "crouch", config.player.crouch, "player", logger);
    config.player.accel = readNumber(player, "accel", config.player.accel, "player", logger);
    config.player.decel = readNumber(player, "decel", config.player.decel, "player", logger);
    config.player.bagSpeedMult =
        readNumber(player, "bag_speed_mult", config.player.bagSpeedMult, "player", logger);
    config.player.hp = readNumber(player, "hp", config.player.hp, "player", logger);
    config.player.armor = readNumber(player, "armor", config.player.armor, "player", logger);
    config.player.armorRegen =
        readNumber(player, "armor_regen", config.player.armorRegen, "player", logger);
    config.player.armorRegenDelay =
        readNumber(player, "armor_regen_delay", config.player.armorRegenDelay, "player", logger);
    const auto& pickup = groupOrEmpty(data, "pickup", empty);
    config.pickup.medkitChance =
        readNumber(pickup, "medkit_chance", config.pickup.medkitChance, "pickup", logger, 1);
    config.pickup.armorChance =
        readNumber(pickup, "armor_chance", config.pickup.armorChance, "pickup", logger, 1);
    if (config.pickup.medkitChance + config.pickup.armorChance > 1) {
        logger.log(LogLevel::Warn, "Invalid pickup probability sum; using defaults");
        config.pickup.medkitChance = PickupConfig{}.medkitChance;
        config.pickup.armorChance = PickupConfig{}.armorChance;
    }
    config.pickup.medkitAmount =
        readNumber(pickup, "medkit_amount", config.pickup.medkitAmount, "pickup", logger);
    config.pickup.armorAmount =
        readNumber(pickup, "armor_amount", config.pickup.armorAmount, "pickup", logger);
    config.pickup.collectRadius =
        readNumber(pickup, "collect_radius", config.pickup.collectRadius, "pickup", logger);
    const auto& enemyCombat = groupOrEmpty(data, "enemy_combat", empty);
    config.enemyCombat.reactionTime = readNumber(
        enemyCombat, "reaction_time", config.enemyCombat.reactionTime, "enemy_combat", logger);
    config.enemyCombat.burstInterval = readNumber(
        enemyCombat, "burst_interval", config.enemyCombat.burstInterval, "enemy_combat", logger);
    if (config.enemyCombat.burstInterval <= 0) {
        logger.log(LogLevel::Warn, "Invalid enemy_combat.burst_interval; using default");
        config.enemyCombat.burstInterval = EnemyCombatConfig{}.burstInterval;
    }
    config.enemyCombat.strafeSpeed = readNumber(
        enemyCombat, "strafe_speed", config.enemyCombat.strafeSpeed, "enemy_combat", logger);
    config.enemyCombat.strafeReverseTime =
        readNumber(enemyCombat, "strafe_reverse_time", config.enemyCombat.strafeReverseTime,
                   "enemy_combat", logger);
    if (config.enemyCombat.strafeReverseTime <= 0) {
        logger.log(LogLevel::Warn, "Invalid enemy_combat.strafe_reverse_time; using default");
        config.enemyCombat.strafeReverseTime = EnemyCombatConfig{}.strafeReverseTime;
    }
    config.enemyCombat.pathRefresh = readNumber(
        enemyCombat, "path_refresh", config.enemyCombat.pathRefresh, "enemy_combat", logger);
    if (config.enemyCombat.pathRefresh <= 0) {
        logger.log(LogLevel::Warn, "Invalid enemy_combat.path_refresh; using default");
        config.enemyCombat.pathRefresh = EnemyCombatConfig{}.pathRefresh;
    }
    config.enemyCombat.deathFade =
        readNumber(enemyCombat, "death_fade", config.enemyCombat.deathFade, "enemy_combat", logger);
    if (config.enemyCombat.deathFade <= 0) {
        logger.log(LogLevel::Warn, "Invalid enemy_combat.death_fade; using default");
        config.enemyCombat.deathFade = EnemyCombatConfig{}.deathFade;
    }
    const auto& view = groupOrEmpty(data, "view", empty);
    config.view.leadPx = readNumber(view, "lead_px", config.view.leadPx, "view", logger);
    config.view.followRate =
        readNumber(view, "follow_rate", config.view.followRate, "view", logger);
    const auto& ui = groupOrEmpty(data, "ui", empty);
    config.ui.hoverTime = readNumber(ui, "hover_time", config.ui.hoverTime, "ui", logger);
    config.ui.transitionTime =
        readNumber(ui, "transition_time", config.ui.transitionTime, "ui", logger);
    if (config.ui.hoverTime <= 0) {
        logger.log(LogLevel::Warn, "Invalid ui.hover_time; using default");
        config.ui.hoverTime = UiConfig{}.hoverTime;
    }
    if (config.ui.transitionTime <= 0) {
        logger.log(LogLevel::Warn, "Invalid ui.transition_time; using default");
        config.ui.transitionTime = UiConfig{}.transitionTime;
    }
    const auto& render = groupOrEmpty(data, "render", empty);
    config.render.ambientFloorAlpha = readNumber(
        render, "ambient_floor_alpha", config.render.ambientFloorAlpha, "render", logger, 1.0f);
    config.render.ambientWallAlpha = readNumber(
        render, "ambient_wall_alpha", config.render.ambientWallAlpha, "render", logger, 1.0f);
    const auto& ping = groupOrEmpty(data, "ping", empty);
    config.ping.smallRadius =
        readNumber(ping, "small_radius", config.ping.smallRadius, "ping", logger);
    config.ping.bigRadius = readNumber(ping, "big_radius", config.ping.bigRadius, "ping", logger);
    config.ping.tapMax = readNumber(ping, "tap_max", config.ping.tapMax, "ping", logger);
    config.ping.chargeMax = readNumber(ping, "charge_max", config.ping.chargeMax, "ping", logger);
    config.ping.speed = readNumber(ping, "speed", config.ping.speed, "ping", logger);
    config.ping.fade = readNumber(ping, "fade", config.ping.fade, "ping", logger);
    config.ping.cooldown = readNumber(ping, "cooldown", config.ping.cooldown, "ping", logger);
    config.ping.noiseMult = readNumber(ping, "noise_mult", config.ping.noiseMult, "ping", logger);
    config.ping.halo = readNumber(ping, "halo", config.ping.halo, "ping", logger);
    config.ping.hazardRevealRadius =
        readNumber(ping, "hazard_reveal_radius", config.ping.hazardRevealRadius, "ping", logger);
    const auto& noise = groupOrEmpty(data, "noise", empty);
    config.noise.crouch = readNumber(noise, "crouch", config.noise.crouch, "noise", logger);
    config.noise.walk = readNumber(noise, "walk", config.noise.walk, "noise", logger);
    config.noise.sprint = readNumber(noise, "sprint", config.noise.sprint, "noise", logger);
    config.noise.lockpick = readNumber(noise, "lockpick", config.noise.lockpick, "noise", logger);
    config.noise.crack = readNumber(noise, "crack", config.noise.crack, "noise", logger);
    config.noise.crackInterval =
        readNumber(noise, "crack_interval", config.noise.crackInterval, "noise", logger);
    config.noise.shotSuppressed =
        readNumber(noise, "shot_suppressed", config.noise.shotSuppressed, "noise", logger);
    config.noise.shot = readNumber(noise, "shot", config.noise.shot, "noise", logger);
    config.noise.laser = readNumber(noise, "laser", config.noise.laser, "noise", logger);
    const auto& guard = groupOrEmpty(data, "guard", empty);
    config.guard.radius = readNumber(guard, "radius", config.guard.radius, "guard", logger);
    config.guard.stationaryTurnSpeed = readNumber(
        guard, "stationary_turn_speed", config.guard.stationaryTurnSpeed, "guard", logger);
    config.guard.patrolSpeed =
        readNumber(guard, "patrol_speed", config.guard.patrolSpeed, "guard", logger);
    config.guard.searchSpeed =
        readNumber(guard, "search_speed", config.guard.searchSpeed, "guard", logger);
    config.guard.chaseSpeed =
        readNumber(guard, "chase_speed", config.guard.chaseSpeed, "guard", logger);
    config.guard.coneDeg = readNumber(guard, "cone_deg", config.guard.coneDeg, "guard", logger);
    config.guard.rangeLit = readNumber(guard, "range_lit", config.guard.rangeLit, "guard", logger);
    config.guard.rangeDim = readNumber(guard, "range_dim", config.guard.rangeDim, "guard", logger);
    config.guard.rangeDark =
        readNumber(guard, "range_dark", config.guard.rangeDark, "guard", logger);
    config.guard.crouchDarkMult =
        readNumber(guard, "crouch_dark_mult", config.guard.crouchDarkMult, "guard", logger);
    config.guard.fillFar = readNumber(guard, "fill_far", config.guard.fillFar, "guard", logger);
    config.guard.fillNear = readNumber(guard, "fill_near", config.guard.fillNear, "guard", logger);
    config.guard.sprintMult =
        readNumber(guard, "sprint_mult", config.guard.sprintMult, "guard", logger);
    config.guard.crouchMult =
        readNumber(guard, "crouch_mult", config.guard.crouchMult, "guard", logger);
    config.guard.decay = readNumber(guard, "decay", config.guard.decay, "guard", logger);
    config.guard.callin = readNumber(guard, "callin", config.guard.callin, "guard", logger);
    config.guard.takedownRange =
        readNumber(guard, "takedown_range", config.guard.takedownRange, "guard", logger);
    config.guard.suspiciousTime =
        readNumber(guard, "suspicious_time", config.guard.suspiciousTime, "guard", logger);
    config.guard.searchTime =
        readNumber(guard, "search_time", config.guard.searchTime, "guard", logger);
    config.guard.investigateLook =
        readNumber(guard, "investigate_look", config.guard.investigateLook, "guard", logger);
    config.guard.searchLoopRadius =
        readNumber(guard, "search_loop_radius", config.guard.searchLoopRadius, "guard", logger);
    config.guard.searchPointPause =
        readNumber(guard, "search_point_pause", config.guard.searchPointPause, "guard", logger);
    config.guard.searchTurnRate =
        readNumber(guard, "search_turn_rate", config.guard.searchTurnRate, "guard", logger);
    config.guard.lookSweepDeg =
        readNumber(guard, "look_sweep_deg", config.guard.lookSweepDeg, "guard", logger);
    config.guard.stuckWindow =
        readNumber(guard, "stuck_window", config.guard.stuckWindow, "guard", logger);
    config.guard.stuckMinProgress =
        readNumber(guard, "stuck_min_progress", config.guard.stuckMinProgress, "guard", logger);
    config.guard.arriveTolerance =
        readNumber(guard, "arrive_tolerance", config.guard.arriveTolerance, "guard", logger);
    config.guard.pathClearStep =
        readNumber(guard, "path_clear_step", config.guard.pathClearStep, "guard", logger);
    config.guard.crumbSpacing =
        readNumber(guard, "crumb_spacing", config.guard.crumbSpacing, "guard", logger);
    config.guard.crumbMax = readNumber(guard, "crumb_max", config.guard.crumbMax, "guard", logger);
    const auto& camera = groupOrEmpty(data, "camera", empty);
    config.camera.coneDeg = readNumber(camera, "cone_deg", config.camera.coneDeg, "camera", logger);
    config.camera.range = readNumber(camera, "range", config.camera.range, "camera", logger);
    config.camera.callin = readNumber(camera, "callin", config.camera.callin, "camera", logger);
    config.camera.loopSeconds =
        readNumber(camera, "loop_seconds", config.camera.loopSeconds, "camera", logger);
    config.camera.sweepSpeed =
        readNumber(camera, "sweep_speed", config.camera.sweepSpeed, "camera", logger);
    const auto& laser = groupOrEmpty(data, "laser", empty);
    config.laser.touchCooldown =
        readNumber(laser, "touch_cooldown", config.laser.touchCooldown, "laser", logger);
    config.laser.secondTouchWindow =
        readNumber(laser, "second_touch_window", config.laser.secondTouchWindow, "laser", logger);
    const auto& pager = groupOrEmpty(data, "pager", empty);
    config.pager.ringDelay =
        readNumber(pager, "ring_delay", config.pager.ringDelay, "pager", logger);
    config.pager.answerWindow =
        readNumber(pager, "answer_window", config.pager.answerWindow, "pager", logger);
    config.pager.answerHold =
        readNumber(pager, "answer_hold", config.pager.answerHold, "pager", logger);
    const auto& alarm = groupOrEmpty(data, "alarm", empty);
    config.alarm.slowmoScale =
        readNumber(alarm, "slowmo_scale", config.alarm.slowmoScale, "alarm", logger);
    config.alarm.slowmoTime =
        readNumber(alarm, "slowmo_time", config.alarm.slowmoTime, "alarm", logger);
    config.alarm.flipTime = readNumber(alarm, "flip_time", config.alarm.flipTime, "alarm", logger);
    config.alarm.barsIn = readNumber(alarm, "bars_in", config.alarm.barsIn, "alarm", logger);
    config.alarm.barsOut = readNumber(alarm, "bars_out", config.alarm.barsOut, "alarm", logger);
    config.alarm.shakeTrauma =
        readNumber(alarm, "shake_trauma", config.alarm.shakeTrauma, "alarm", logger);
    config.alarm.traumaDecay =
        readNumber(alarm, "trauma_decay", config.alarm.traumaDecay, "alarm", logger);
    config.alarm.bannerTime =
        readNumber(alarm, "banner_time", config.alarm.bannerTime, "alarm", logger);
    config.alarm.shakePixels =
        readNumber(alarm, "shake_pixels", config.alarm.shakePixels, "alarm", logger);
    config.alarm.firstWaveDelay =
        readNumber(alarm, "first_wave_delay", config.alarm.firstWaveDelay, "alarm", logger);
    config.alarm.waveInterval =
        readNumber(alarm, "wave_interval", config.alarm.waveInterval, "alarm", logger);
    if (config.alarm.waveInterval <= 0) {
        logger.log(LogLevel::Warn, "Invalid alarm.wave_interval; using default");
        config.alarm.waveInterval = AlarmConfig{}.waveInterval;
    }
    config.alarm.maxAlive = readNumber(alarm, "max_alive", config.alarm.maxAlive, "alarm", logger);
    const auto& mission = groupOrEmpty(data, "mission", empty);
    config.mission.lockpick =
        readNumber(mission, "lockpick", config.mission.lockpick, "mission", logger);
    config.mission.securityHold =
        readNumber(mission, "security_hold", config.mission.securityHold, "mission", logger);
    config.mission.breakerHold =
        readNumber(mission, "breaker_hold", config.mission.breakerHold, "mission", logger);
    config.mission.crack = readNumber(mission, "crack", config.mission.crack, "mission", logger);
    config.mission.thermitePlace =
        readNumber(mission, "thermite_place", config.mission.thermitePlace, "mission", logger);
    config.mission.thermiteBurn =
        readNumber(mission, "thermite_burn", config.mission.thermiteBurn, "mission", logger);
    config.mission.dyeHold =
        readNumber(mission, "dye_hold", config.mission.dyeHold, "mission", logger);
    config.mission.dyeBurst =
        readNumber(mission, "dye_burst", config.mission.dyeBurst, "mission", logger);
    config.mission.bollardHold =
        readNumber(mission, "bollard_hold", config.mission.bollardHold, "mission", logger);
    config.mission.vanDelay =
        readNumber(mission, "van_delay", config.mission.vanDelay, "mission", logger);
    config.mission.throwDistance =
        readNumber(mission, "throw_distance", config.mission.throwDistance, "mission", logger);
    const auto& retry = groupOrEmpty(mission, "retry_positions", empty);
    for (std::size_t i = 0; i < config.mission.retryPositions.size(); ++i) {
        const std::string key = "s" + std::to_string(i + 3);
        const auto point = retry.find(key);
        if (point != retry.end() && point->is_array() && point->size() == 2 &&
            (*point)[0].is_number_integer() && (*point)[1].is_number_integer() &&
            (*point)[0].get<double>() >= 0 && (*point)[1].get<double>() >= 0 &&
            (*point)[0].get<double>() <= std::numeric_limits<int>::max() &&
            (*point)[1].get<double>() <= std::numeric_limits<int>::max()) {
            config.mission.retryPositions[i] = {(*point)[0].get<int>(), (*point)[1].get<int>()};
        } else {
            logger.log(LogLevel::Warn, "Missing or invalid tuning key: mission.retry_positions." +
                                           key + "; using default");
        }
    }
    const auto& payout = groupOrEmpty(data, "payout", empty);
    const auto readPayoutRate = [&](const char* key, double fallback) {
        const auto entry = payout.find(key);
        if (entry != payout.end() && entry->is_number()) {
            const double value = entry->get<double>();
            if (std::isfinite(value) && value >= 0 && value <= std::numeric_limits<float>::max())
                return value;
        }
        logger.log(LogLevel::Warn,
                   std::string("Missing or invalid tuning key: payout.") + key + "; using default");
        return fallback;
    };
    config.payout.bag = readNumber(payout, "bag", config.payout.bag, "payout", logger);
    config.payout.spoiled = readNumber(payout, "spoiled", config.payout.spoiled, "payout", logger);
    config.payout.ghostBonus = readPayoutRate("ghost_bonus", config.payout.ghostBonus);
    config.payout.timeBonus =
        readNumber(payout, "time_bonus", config.payout.timeBonus, "payout", logger);
    config.payout.timeBonusLimit =
        readNumber(payout, "time_bonus_limit", config.payout.timeBonusLimit, "payout", logger);
    config.payout.handlerCut = readPayoutRate("handler_cut", config.payout.handlerCut);
    config.payout.deathPenalty =
        readNumber(payout, "death_penalty", config.payout.deathPenalty, "payout", logger);
    config.payout.rankS = readNumber(payout, "rank_s", config.payout.rankS, "payout", logger);
    config.payout.rankA = readNumber(payout, "rank_a", config.payout.rankA, "payout", logger);
    config.payout.rankB = readNumber(payout, "rank_b", config.payout.rankB, "payout", logger);
    const auto readPayoutInteger = [&](const char* key, int fallback) {
        const auto entry = payout.find(key);
        if (entry != payout.end() && entry->is_number()) {
            const double value = entry->get<double>();
            if (std::isfinite(value) && value >= 0 && value <= std::numeric_limits<int>::max() &&
                std::floor(value) == value)
                return static_cast<int>(value);
        }
        logger.log(LogLevel::Warn,
                   std::string("Missing or invalid tuning key: payout.") + key + "; using default");
        return fallback;
    };
    config.payout.deductionMaxCount =
        readPayoutInteger("deduction_max_count", config.payout.deductionMaxCount);
    config.payout.deductionMaxAmount =
        readPayoutInteger("deduction_max_amount", config.payout.deductionMaxAmount);
    const auto& difficulty = groupOrEmpty(data, "difficulty", empty);
    const auto& easy = groupOrEmpty(difficulty, "easy", empty);
    config.difficulty.easy.enemyDmg =
        readNumber(easy, "enemy_dmg", config.difficulty.easy.enemyDmg, "difficulty.easy", logger);
    config.difficulty.easy.detectFill = readNumber(
        easy, "detect_fill", config.difficulty.easy.detectFill, "difficulty.easy", logger);
    config.difficulty.easy.ammo =
        readNumber(easy, "ammo", config.difficulty.easy.ammo, "difficulty.easy", logger);
    config.difficulty.easy.maxAlive =
        readNumber(easy, "max_alive", config.difficulty.easy.maxAlive, "difficulty.easy", logger);
    const auto& normal = groupOrEmpty(difficulty, "normal", empty);
    config.difficulty.normal.enemyDmg = readNumber(
        normal, "enemy_dmg", config.difficulty.normal.enemyDmg, "difficulty.normal", logger);
    config.difficulty.normal.detectFill = readNumber(
        normal, "detect_fill", config.difficulty.normal.detectFill, "difficulty.normal", logger);
    config.difficulty.normal.ammo =
        readNumber(normal, "ammo", config.difficulty.normal.ammo, "difficulty.normal", logger);
    config.difficulty.normal.maxAlive = readNumber(
        normal, "max_alive", config.difficulty.normal.maxAlive, "difficulty.normal", logger);
    const auto& hard = groupOrEmpty(difficulty, "hard", empty);
    config.difficulty.hard.enemyDmg =
        readNumber(hard, "enemy_dmg", config.difficulty.hard.enemyDmg, "difficulty.hard", logger);
    config.difficulty.hard.detectFill = readNumber(
        hard, "detect_fill", config.difficulty.hard.detectFill, "difficulty.hard", logger);
    config.difficulty.hard.ammo =
        readNumber(hard, "ammo", config.difficulty.hard.ammo, "difficulty.hard", logger);
    config.difficulty.hard.maxAlive =
        readNumber(hard, "max_alive", config.difficulty.hard.maxAlive, "difficulty.hard", logger);
    logger.log(LogLevel::Info, "Loaded tuning: " + path);
    return config;
}
