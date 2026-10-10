#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "render/VisibilityFan.h"
namespace {
TileMap room() {
    std::istringstream input("########\n#...#..#\n#...d..#\n#...#..#\n########\n");
    return TileMap::parse(input, 48);
}
}  // namespace
TEST(VisibilityFan, ClosedDoorClipsEveryVertexAndOpenDoorExposesRoom) {
    auto map = room();
    const Vec2 origin{120, 120};
    auto fan = visibilityFan(origin, 0, .5f, {300, 300, 300}, map);
    ASSERT_FALSE(fan.empty());
    for (auto tri : fan)
        for (auto p : {tri.a, tri.b, tri.c}) EXPECT_LE(p.x, 192.001f);
    map.setOpen(4, 2, true);
    fan = visibilityFan(origin, 0, .5f, {300, 300, 300}, map);
    bool beyond = false;
    for (auto tri : fan)
        for (auto p : {tri.a, tri.b, tri.c}) beyond |= p.x > 240;
    EXPECT_TRUE(beyond);
}
TEST(VisibilityFan, DarkRangeLimitsFootprintWithoutMutatingLightOrDoor) {
    auto map = room();
    map.setOpen(4, 2, true);
    map.fillLight(LightLevel::Dark);
    const Vec2 origin{120, 120};
    const auto fan = visibilityFan(origin, 0, .5f, {300, 240, 70}, map);
    ASSERT_FALSE(fan.empty());
    for (auto tri : fan)
        for (auto p : {tri.a, tri.b, tri.c})
            EXPECT_LE(std::hypot(p.x - origin.x, p.y - origin.y), 70.001f);
    EXPECT_TRUE(map.isOpen(4, 2));
    EXPECT_EQ(map.light(2, 2), LightLevel::Dark);
}
TEST(VisibilityFan, InvalidGeometryProducesNoTriangles) {
    auto map = room();
    EXPECT_TRUE(visibilityFan({120, 120}, 0, .5f, {300, -1, 180}, map).empty());
    EXPECT_TRUE(visibilityFan({120, 120}, std::numeric_limits<float>::quiet_NaN(), .5f,
                              {300, 240, 180}, map)
                    .empty());
}
