#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace comskip::ui {
enum class Key : int {
    escape = 27, space = 32, period = '.',
    left = 256, right, up, down, page_up, page_down, home, end,
    insert, delete_key, shift, f1, f2, f3, f4, f5
};

struct KeyEvent { Key key{}; bool pressed{true}; bool shift{}; bool alt{}; };
struct MouseEvent {
    enum class Kind { move, press, release };
    Kind kind{};
    int x{};
    int y{};
    bool left_button{true};
};
struct ResizeEvent { int width{}; int height{}; };
struct QuitEvent {};
using Event = std::variant<KeyEvent, MouseEvent, ResizeEvent, QuitEvent>;

// Key codes retain the review controller's existing ASCII/arrow/F-key contract.
// The state belongs to one window; consume_input clears transient key/click data.
struct InputState {
    int key{};
    int mouse_x{};
    int mouse_y{};
    bool mouse_pressed{};
    bool mouse_down{};
    bool shift{};
    bool alt{};
    bool quit_requested{};
};

struct WindowOptions {
    int width{800};
    int height{600};
    std::string title{"Comskip"};
    std::filesystem::path font_path;
    int font_size{16};
    bool hidden{};
};

class ReviewWindow {
    struct Impl;
    std::unique_ptr<Impl> implementation_;
    WindowOptions options_;
    InputState input_;
    int window_width_{};
    int window_height_{};
    std::string overlay_text_;

    void present();
public:
    explicit ReviewWindow(WindowOptions options = {});
    ~ReviewWindow();
    ReviewWindow(ReviewWindow&&) noexcept;
    ReviewWindow& operator=(ReviewWindow&&) noexcept;
    ReviewWindow(const ReviewWindow&) = delete;
    ReviewWindow& operator=(const ReviewWindow&) = delete;

    static bool available() noexcept;
    bool is_open() const noexcept;
    const WindowOptions& options() const noexcept { return options_; }
    InputState& input() noexcept { return input_; }
    const InputState& input() const noexcept { return input_; }
    InputState consume_input() noexcept;
    const std::string& overlay_text() const noexcept { return overlay_text_; }
    std::uint32_t window_id() const noexcept;

    void open();
    // Configure a closed window without replacing its controller/input state.
    void configure_font(std::filesystem::path path, int size);
    void open(int width, int height, std::string_view title);
    // Input is RGB24, row-major. A zero pitch selects width * 3.
    void draw(std::span<const std::uint8_t> rgb, int pitch = 0);
    void refresh();
    void wait();
    void close() noexcept;
    void show_help(std::span<const std::string_view> lines);
    void show_details(std::string_view text);
    void clear_text();
    // Explicit event injection supports controller tests without a video driver.
    void process_event(const Event& event);
};
}
