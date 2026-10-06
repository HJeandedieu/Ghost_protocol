#include <gtest/gtest.h>

#include <sstream>

#include "entities/Laser.h"
#include "entities/SecurityCamera.h"
#include "systems/RippleSystem.h"

TEST(HazardReveal, ProximityPinsCameraAtInclusiveConfiguredRadiusThenFadesNormally) {
    std::istringstream source("..........\n..........\n..........\n..........\n..........\n");
    const auto map = TileMap::parse(source, 48);
    CameraSpawn spawn;
    spawn.position = {4, 2};
    SecurityCamera camera(spawn, map, CameraConfig{});
    std::vector<Entity*> hazards{&camera};
    PingConfig config;
    RippleSystem ripple(config, map);
    ripple.applyProximity({96, 120}, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 1);
    ripple.update(0.5f, map, hazards);
    ripple.applyProximity({95, 120}, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 0.8f);
    ripple.update(2, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 0);
    config.hazardRevealRadius = 50;
    RippleSystem smaller(config, map);
    smaller.applyProximity({165, 120}, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 0);
    smaller.applyProximity({166, 120}, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 1);
}

TEST(HazardReveal, WallBlocksCameraRevealAndLaserUsesNearestSegmentPoint) {
    std::istringstream source("..........\n..#.......\n..........\n..........\n..........\n");
    const auto map = TileMap::parse(source, 48);
    CameraSpawn spawn;
    spawn.position = {3, 1};
    SecurityCamera camera(spawn, map, CameraConfig{});
    Laser laser(LaserSpawn{"L01", {1, 3}, {8, 3}}, map);
    std::vector<Entity*> hazards{&camera, &laser};
    RippleSystem ripple(PingConfig{}, map);
    ripple.applyProximity({72, 72}, map, hazards);
    EXPECT_FLOAT_EQ(camera.reveal, 0);
    ripple.applyProximity({360, 168}, map, hazards);
    EXPECT_FLOAT_EQ(laser.reveal, 1);
}

TEST(HazardReveal, PingRevealsMidBeamWithoutProximityAndLeavesOtherHazardsDark) {
    std::istringstream source("..........\n..........\n..........\n..........\n..........\n");
    const auto map = TileMap::parse(source, 48);
    Laser laser(LaserSpawn{"L01", {1, 3}, {8, 3}}, map);
    CameraSpawn spawn;
    spawn.position = {0, 0};
    SecurityCamera camera(spawn, map, CameraConfig{});
    std::vector<Entity*> hazards{&laser, &camera};
    RippleSystem ripple(PingConfig{}, map);
    ripple.startPing({360, 24}, 0);
    ripple.update(0.2f, map, hazards);
    EXPECT_GT(laser.reveal, 0);
    EXPECT_FLOAT_EQ(camera.reveal, 0);
}
