#include <gtest/gtest.h>

#include <cmath>

#include "platform/Viewport.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};       // 4:3
    constexpr math::Vec2 WIDE{2560.0f, 1440.0f};      // 16:9, so bars left and right
    constexpr math::Vec2 TALL{800.0f, 800.0f};        // square, so bars top and bottom
    constexpr float TOLERANCE = 1e-3f;
}

TEST(FitViewport, MatchingAspectFillsTheFramebuffer)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, math::Vec2{1600.0f, 1200.0f});

    EXPECT_NEAR(viewport.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(viewport.y, 0.0f, TOLERANCE);
    EXPECT_NEAR(viewport.width, 1600.0f, TOLERANCE);
    EXPECT_NEAR(viewport.height, 1200.0f, TOLERANCE);
}

TEST(FitViewport, AWiderFramebufferGetsBarsAtTheSides)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, WIDE);

    EXPECT_NEAR(viewport.width, 1920.0f, TOLERANCE);
    EXPECT_NEAR(viewport.height, 1440.0f, TOLERANCE);
    EXPECT_NEAR(viewport.x, 320.0f, TOLERANCE);
    EXPECT_NEAR(viewport.y, 0.0f, TOLERANCE);
}

TEST(FitViewport, ATallerFramebufferGetsBarsAboveAndBelow)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, TALL);

    EXPECT_NEAR(viewport.width, 800.0f, TOLERANCE);
    EXPECT_NEAR(viewport.height, 600.0f, TOLERANCE);
    EXPECT_NEAR(viewport.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(viewport.y, 100.0f, TOLERANCE);
}

// Whatever the display, a square in the world stays square on screen.
TEST(FitViewport, PreservesTheWorldAspectRatio)
{
    const math::Vec2 framebuffers[] = {WIDE, TALL, math::Vec2{1024.0f, 768.0f},
                                       math::Vec2{3440.0f, 1440.0f}};

    for (const math::Vec2 framebuffer : framebuffers)
    {
        const platform::Viewport viewport = platform::fit_viewport(WORLD, framebuffer);

        EXPECT_NEAR(viewport.width / viewport.height, WORLD.x / WORLD.y, TOLERANCE);
        EXPECT_LE(viewport.width, framebuffer.x + TOLERANCE);
        EXPECT_LE(viewport.height, framebuffer.y + TOLERANCE);
    }
}

TEST(WorldFromPixel, TheCentreOfTheViewportIsTheCentreOfTheWorld)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, WIDE);
    const math::Vec2 point = platform::world_from_pixel(math::Vec2{1280.0f, 720.0f}, WIDE, viewport, WORLD);

    EXPECT_NEAR(point.x, 400.0f, TOLERANCE);
    EXPECT_NEAR(point.y, 300.0f, TOLERANCE);
}

// The cursor is reported with y down from the top; the world has y up. Getting
// this backwards puts every click in the wrong half of the screen.
TEST(WorldFromPixel, TheTopOfTheScreenIsTheTopOfTheWorld)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, WIDE);

    const math::Vec2 top = platform::world_from_pixel(math::Vec2{1280.0f, 0.0f}, WIDE, viewport, WORLD);
    const math::Vec2 bottom = platform::world_from_pixel(math::Vec2{1280.0f, 1440.0f}, WIDE, viewport, WORLD);

    EXPECT_NEAR(top.y, 600.0f, TOLERANCE);
    EXPECT_NEAR(bottom.y, 0.0f, TOLERANCE);
}

TEST(WorldFromPixel, TheViewportCornersAreTheWorldCorners)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, WIDE);

    const math::Vec2 bottom_left = platform::world_from_pixel(math::Vec2{320.0f, 1440.0f}, WIDE, viewport, WORLD);
    const math::Vec2 top_right = platform::world_from_pixel(math::Vec2{2240.0f, 0.0f}, WIDE, viewport, WORLD);

    EXPECT_NEAR(bottom_left.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(bottom_left.y, 0.0f, TOLERANCE);
    EXPECT_NEAR(top_right.x, 800.0f, TOLERANCE);
    EXPECT_NEAR(top_right.y, 600.0f, TOLERANCE);
}

// A click in a black bar lands outside the world, which is what the caller's
// own bounds check rejects. No separate flag is needed to say so.
TEST(WorldFromPixel, PixelsInTheSideBarsFallOutsideTheWorld)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, WIDE);

    EXPECT_LT(platform::world_from_pixel(math::Vec2{100.0f, 720.0f}, WIDE, viewport, WORLD).x, 0.0f);
    EXPECT_GT(platform::world_from_pixel(math::Vec2{2500.0f, 720.0f}, WIDE, viewport, WORLD).x, WORLD.x);
}

TEST(WorldFromPixel, PixelsInTheTopAndBottomBarsFallOutsideTheWorld)
{
    const platform::Viewport viewport = platform::fit_viewport(WORLD, TALL);

    EXPECT_GT(platform::world_from_pixel(math::Vec2{400.0f, 50.0f}, TALL, viewport, WORLD).y, WORLD.y);
    EXPECT_LT(platform::world_from_pixel(math::Vec2{400.0f, 750.0f}, TALL, viewport, WORLD).y, 0.0f);

    const math::Vec2 middle = platform::world_from_pixel(math::Vec2{400.0f, 400.0f}, TALL, viewport, WORLD);
    EXPECT_GE(middle.y, 0.0f);
    EXPECT_LE(middle.y, WORLD.y);
}

// A viewport is empty until set_viewport runs. Dividing by it would put NaN
// into every position that followed.
TEST(WorldFromPixel, ADegenerateViewportDoesNotProduceNaN)
{
    const math::Vec2 point =
        platform::world_from_pixel(math::Vec2{10.0f, 10.0f}, WIDE, platform::Viewport{}, WORLD);

    EXPECT_TRUE(std::isfinite(point.x));
    EXPECT_TRUE(std::isfinite(point.y));
}
