#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "world/Raycast.h"
#include "world/TileMap.h"

TEST(Raycast3D, PrismSlabsHandleParallelInsideBehindAndCornerHits) {
    const Vec3 low{10, 0, 10}, high{20, 64, 20};
    EXPECT_FLOAT_EQ(*Raycast::intersectPrism({{0, 36, 15}, {1, 0, 0}}, low, high), 10);
    EXPECT_FLOAT_EQ(*Raycast::intersectPrism({{15, 36, 15}, {-1, 0, 0}}, low, high), 0);
    EXPECT_FALSE(Raycast::intersectPrism({{0, 65, 15}, {1, 0, 0}}, low, high));
    EXPECT_FALSE(Raycast::intersectPrism({{0, 36, 15}, {-1, 0, 0}}, low, high));
    EXPECT_NEAR(*Raycast::intersectPrism({{0, 36, 0}, {1, 0, 1}}, low, high), std::sqrt(200.f),
                .0001);
}

TEST(Raycast3D, CylinderIncludesSideCapsTangentsAndOriginInside) {
    EXPECT_FLOAT_EQ(*Raycast::intersectCylinder({{0, 36, 0}, {1, 0, 0}}, {100, 0}, 14, 48), 86);
    EXPECT_FLOAT_EQ(*Raycast::intersectCylinder({{100, 60, 0}, {0, -1, 0}}, {100, 0}, 14, 48), 12);
    EXPECT_FLOAT_EQ(*Raycast::intersectCylinder({{100, -10, 0}, {0, 1, 0}}, {100, 0}, 14, 48), 10);
    EXPECT_FLOAT_EQ(*Raycast::intersectCylinder({{100, 20, 0}, {1, 0, 0}}, {100, 0}, 14, 48), 0);
    EXPECT_FLOAT_EQ(*Raycast::intersectCylinder({{0, 36, 14}, {1, 0, 0}}, {100, 0}, 14, 48), 100);
    EXPECT_FALSE(Raycast::intersectCylinder({{0, 49, 0}, {1, 0, 0}}, {100, 0}, 14, 48));
    EXPECT_FALSE(Raycast::intersectCylinder({{0, 36, 15}, {1, 0, 0}}, {100, 0}, 14, 48));
}

TEST(Raycast3D, InvalidRaysAndGeometryNeverProduceAnIntersection) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const ShotRay ray : {ShotRay{{0, 36, 0}, {}}, ShotRay{{nan, 36, 0}, {1, 0, 0}},
                              ShotRay{{0, 36, 0}, {1, nan, 0}}}) {
        EXPECT_FALSE(Raycast::intersectCylinder(ray, {100, 0}, 14, 48));
        EXPECT_FALSE(Raycast::intersectPrism(ray, {10, 0, 0}, {20, 64, 48}));
        EXPECT_FALSE(Raycast::intersectFloor(ray));
        EXPECT_FALSE(Raycast::intersectShield(ray, {100, 0}, 0, {}));
    }
    EXPECT_FALSE(Raycast::intersectCylinder({{0, 36, 0}, {1, 0, 0}}, {100, 0}, -1, 48));
    EXPECT_FALSE(Raycast::intersectPrism({{0, 36, 0}, {1, 0, 0}}, {20, 0, 0}, {10, 64, 48}));
}

TEST(Raycast3D, FloorBlocksDownwardShotsButAddsNoCeiling) {
    EXPECT_FLOAT_EQ(*Raycast::intersectFloor({{0, 36, 0}, {0, -1, 0}}), 36);
    EXPECT_FALSE(Raycast::intersectFloor({{0, 36, 0}, {0, 1, 0}}));
    EXPECT_FALSE(Raycast::intersectFloor({{0, 36, 0}, {1, 0, 0}}));
    EXPECT_FLOAT_EQ(*Raycast::intersectFloor({{0, 0, 0}, {1, 0, 0}}), 0);
}

TEST(Raycast3D, ShieldPlateRotatesAndRejectsUncoveredHeightsAndWidths) {
    ShotGeometryConfig geometry;
    EXPECT_FLOAT_EQ(
        *Raycast::intersectShield({{200, 24, 100}, {-1, 0, 0}}, {100, 100}, 0, geometry), 86);
    EXPECT_FLOAT_EQ(*Raycast::intersectShield({{100, 24, 200}, {0, 0, -1}}, {100, 100},
                                              3.14159265358979323846f / 2, geometry),
                    86);
    EXPECT_FALSE(Raycast::intersectShield({{200, 47, 100}, {-1, 0, 0}}, {100, 100}, 0, geometry));
    EXPECT_FALSE(Raycast::intersectShield({{200, 1, 100}, {-1, 0, 0}}, {100, 100}, 0, geometry));
    EXPECT_FALSE(Raycast::intersectShield({{200, 24, 117}, {-1, 0, 0}}, {100, 100}, 0, geometry));
    EXPECT_FLOAT_EQ(*Raycast::intersectShield({{114, 24, 70}, {0, 0, 1}}, {100, 100}, 0, geometry),
                    14);
}

TEST(Raycast3D, WallsDoorsFloorAndRangeShareTheSameBlockingTrace) {
    for (char tile : std::string("#dSRGVF")) {
        std::istringstream source(std::string("..") + tile + "...");
        auto map = TileMap::parse(source, 48);
        const ShotRay ray{{24, 36, 24}, {1, 0, 0}};
        EXPECT_FLOAT_EQ(*Raycast::blockingDistance(ray, 300, map, 64), 72) << tile;
        EXPECT_FALSE(Raycast::blockingDistance(ray, 71, map, 64));
        EXPECT_FLOAT_EQ(*Raycast::blockingDistance({{100, 36, 24}, {1, 0, 0}}, 300, map, 64), 0);
        EXPECT_FALSE(Raycast::blockingDistance({{24, 65, 24}, {1, 0, 0}}, 300, map, 64));
        if (tile != '#') {
            map.setOpen(2, 0, true);
            EXPECT_FALSE(Raycast::blockingDistance(ray, 300, map, 64));
        }
        EXPECT_FLOAT_EQ(*Raycast::blockingDistance({{24, 36, 24}, {0, -1, 0}}, 300, map, 64), 36);
    }
}
