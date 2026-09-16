#include "review_window.h"

#include <gtest/gtest.h>
#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if COMSKIP_BUILD_GUI
#include <SDL.h>
#endif

using namespace comskip::ui;

TEST(ReviewWindow, OwnsIndependentInputAndConsumesTransientEvents)
{
    ReviewWindow first, second;
    first.process_event(KeyEvent{static_cast<Key>('w')});
    first.process_event(MouseEvent{MouseEvent::Kind::press, 10, 20});
    const auto input = first.consume_input();
    EXPECT_EQ(input.key, 'W');
    EXPECT_TRUE(input.mouse_pressed);
    EXPECT_TRUE(first.input().mouse_down);
    EXPECT_EQ(first.input().key, 0);
    EXPECT_FALSE(first.input().mouse_pressed);
    EXPECT_EQ(second.input().key, 0);
    EXPECT_FALSE(second.input().mouse_down);
}

TEST(ReviewWindow, PreservesNavigationFunctionAndModifierMappings)
{
    ReviewWindow window;
    const std::array cases{
        std::pair{KeyEvent{Key::left}, 37}, std::pair{KeyEvent{Key::right}, 39},
        std::pair{KeyEvent{Key::left, true, true}, int('P')},
        std::pair{KeyEvent{Key::right, true, true}, int('N')},
        std::pair{KeyEvent{Key::page_up, true, false, true}, 133},
        std::pair{KeyEvent{Key::page_down, true, false, true}, 134},
        std::pair{KeyEvent{Key::f1}, 112}, std::pair{KeyEvent{Key::f5}, 116},
        std::pair{KeyEvent{Key::insert}, int('I')}, std::pair{KeyEvent{Key::delete_key}, int('D')}
    };
    for (const auto& [event, expected] : cases) {
        window.process_event(event);
        EXPECT_EQ(window.consume_input().key, expected);
    }
    window.process_event(KeyEvent{Key::shift});
    EXPECT_TRUE(window.input().shift);
    window.process_event(KeyEvent{Key::shift, false});
    EXPECT_FALSE(window.input().shift);
}

TEST(ReviewWindow, ScalesAndClampsMouseCoordinatesAfterResizeAndTracksDragging)
{
    ReviewWindow window(WindowOptions{800, 600});
    window.process_event(ResizeEvent{1600, 1200});
    window.process_event(MouseEvent{MouseEvent::Kind::press, 800, 600});
    EXPECT_EQ(window.input().mouse_x, 400);
    EXPECT_EQ(window.input().mouse_y, 300);
    window.consume_input();
    window.process_event(MouseEvent{MouseEvent::Kind::move, 2000, -10});
    EXPECT_EQ(window.input().mouse_x, 799);
    EXPECT_EQ(window.input().mouse_y, 0);
    EXPECT_TRUE(window.input().mouse_pressed);
    window.process_event(MouseEvent{MouseEvent::Kind::release, 2000, -10});
    EXPECT_FALSE(window.input().mouse_down);
}

TEST(ReviewWindow, QuitIsExplicitAndDoesNotTerminateTheProcess)
{
    ReviewWindow window;
    window.process_event(QuitEvent{});
    EXPECT_TRUE(window.input().quit_requested);
    EXPECT_EQ(window.input().key, 27);
    window.close();
    EXPECT_FALSE(window.input().quit_requested);
}

TEST(ReviewWindow, ValidatesDimensionsAndClosedWindowOperations)
{
    EXPECT_THROW(ReviewWindow(WindowOptions{0, 600}), std::invalid_argument);
    ReviewWindow window;
    EXPECT_THROW(window.draw({}), std::logic_error);
    EXPECT_THROW(window.wait(), std::logic_error);
    EXPECT_THROW(window.show_details("details"), std::logic_error);
    static_assert(!std::is_copy_constructible_v<ReviewWindow>);
    static_assert(std::is_nothrow_move_constructible_v<ReviewWindow>);
}
TEST(ReviewWindow, FontConfigurationPreservesControllerStateAndRejectsInvalidMutation) {
    ReviewWindow window;
    window.process_event(KeyEvent{static_cast<Key>('w')});
    window.process_event(MouseEvent{MouseEvent::Kind::press, 10, 20});
    window.configure_font(std::filesystem::u8path("café.ttf"), 22);
    EXPECT_EQ(window.input().key, 'W'); EXPECT_TRUE(window.input().mouse_down);
    EXPECT_EQ(window.options().font_size, 22);
    EXPECT_THROW(window.configure_font({}, 0), std::invalid_argument);
    EXPECT_EQ(window.options().font_size, 22);
    EXPECT_EQ(window.options().font_path, std::filesystem::u8path("café.ttf"));
}

#if !COMSKIP_BUILD_GUI
TEST(ReviewWindow, HeadlessBuildReportsHowToEnableTheUI)
{
    ReviewWindow window;
    EXPECT_FALSE(window.available());
    try { window.open(); FAIL() << "Headless window unexpectedly opened"; }
    catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("COMSKIP_BUILD_GUI=ON"), std::string::npos);
    }
    EXPECT_FALSE(window.is_open());
}
#else
TEST(ReviewWindow, DummyDriverSupportsLifecycleRenderingEventsAndText)
{
    // Run this test with SDL_VIDEODRIVER=dummy; no visible window is required.
    WindowOptions options{160, 120, "Review café"};
    options.hidden = true;
    ReviewWindow window(options);
    window.open();
    EXPECT_TRUE(window.is_open());
    EXPECT_NE(window.window_id(), 0u);
    EXPECT_EQ(window.options().title, "Review café");
    std::vector<std::uint8_t> pixels(160 * 120 * 3, 127);
    window.draw(pixels);
    EXPECT_THROW(window.draw(std::span{pixels}.first(10)), std::invalid_argument);
    EXPECT_THROW(window.draw(pixels, 100), std::invalid_argument);
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.windowID = window.window_id();
    event.key.keysym.sym = SDLK_F1;
    ASSERT_GT(SDL_PushEvent(&event), 0);
    window.refresh();
    EXPECT_EQ(window.consume_input().key, 112);
    const std::array<std::string_view, 3> help{"Review help", "", "W saves the cutlist"};
    window.show_help(help);
    EXPECT_EQ(window.overlay_text(), "Review help\n\nW saves the cutlist");
    window.show_details("Frame 25\nBrightness 127");
    EXPECT_EQ(window.overlay_text(), "Frame 25\nBrightness 127");
    window.show_details("Frame 25");
    auto* renderer = SDL_GetRenderer(SDL_GetWindowFromID(window.window_id()));
    ASSERT_NE(renderer, nullptr);
    const SDL_Rect bottom_pixel{80, 115, 1, 1};
    std::array<std::uint8_t, 3> visible_pixel{};
    ASSERT_EQ(SDL_RenderReadPixels(renderer, &bottom_pixel, SDL_PIXELFORMAT_RGB24,
        visible_pixel.data(), 3), 0) << SDL_GetError();
    EXPECT_EQ(visible_pixel, (std::array<std::uint8_t, 3>{127, 127, 127}));
    window.clear_text();
    EXPECT_TRUE(window.overlay_text().empty());
    ReviewWindow moved(std::move(window));
    EXPECT_FALSE(window.is_open());
    EXPECT_TRUE(moved.is_open());
    moved.close();
    moved.open();
    moved.close();
}
#endif
