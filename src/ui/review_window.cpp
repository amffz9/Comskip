#include "review_window.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if COMSKIP_BUILD_GUI
#include <SDL.h>
#include <SDL_ttf.h>
#endif

namespace comskip::ui {
namespace {
void validate_options(const WindowOptions& options)
{
    if (options.width <= 0 || options.height <= 0 || options.width > std::numeric_limits<int>::max() / 3)
        throw std::invalid_argument("Review window dimensions must be positive and fit an RGB row");
    if (options.font_size <= 0) throw std::invalid_argument("Review font size must be positive");
}

int controller_key(const KeyEvent& event)
{
    switch (event.key) {
    case Key::left: return event.shift ? 'P' : 37;
    case Key::right: return event.shift ? 'N' : 39;
    case Key::up: return 38;
    case Key::down: return 40;
    case Key::page_up: return event.alt ? 133 : 33;
    case Key::page_down: return event.alt ? 134 : 34;
    case Key::home: return 'B';
    case Key::end: return 'E';
    case Key::insert: return 'I';
    case Key::delete_key: return 'D';
    case Key::shift: return 16;
    case Key::f1: return 112;
    case Key::f2: return 113;
    case Key::f3: return 114;
    case Key::f4: return 115;
    case Key::f5: return 116;
    default:
        const int symbol = static_cast<int>(event.key);
        return symbol >= 'a' && symbol <= 'z' ? symbol - 'a' + 'A' : symbol;
    }
}

#if COMSKIP_BUILD_GUI
[[noreturn]] void fail_sdl(std::string_view operation)
{
    throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

template<class T, void(*Release)(T*)>
using SdlOwner = std::unique_ptr<T, decltype(Release)>;

Key sdl_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT: return Key::left;
    case SDLK_RIGHT: return Key::right;
    case SDLK_UP: return Key::up;
    case SDLK_DOWN: return Key::down;
    case SDLK_PAGEUP: return Key::page_up;
    case SDLK_PAGEDOWN: return Key::page_down;
    case SDLK_HOME: return Key::home;
    case SDLK_END: return Key::end;
    case SDLK_INSERT: return Key::insert;
    case SDLK_DELETE: return Key::delete_key;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return Key::shift;
    case SDLK_F1: return Key::f1;
    case SDLK_F2: return Key::f2;
    case SDLK_F3: return Key::f3;
    case SDLK_F4: return Key::f4;
    case SDLK_F5: return Key::f5;
    default: return static_cast<Key>(key);
    }
}
#endif
}

struct ReviewWindow::Impl {
#if COMSKIP_BUILD_GUI
    struct VideoSubsystem {
        VideoSubsystem() {
            // The application owns its standard/wide main entry point.
            SDL_SetMainReady();
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) fail_sdl("Cannot initialize SDL video");
        }
        ~VideoSubsystem() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
    } video;
    struct FontSubsystem {
        FontSubsystem() { if (TTF_Init() != 0) throw std::runtime_error(std::string("Cannot initialize review fonts: ") + TTF_GetError()); }
        ~FontSubsystem() { TTF_Quit(); }
    } fonts;
    SdlOwner<SDL_Window, SDL_DestroyWindow> window{nullptr, SDL_DestroyWindow};
    SdlOwner<SDL_Renderer, SDL_DestroyRenderer> renderer{nullptr, SDL_DestroyRenderer};
    SdlOwner<SDL_Texture, SDL_DestroyTexture> image{nullptr, SDL_DestroyTexture};
    SdlOwner<TTF_Font, TTF_CloseFont> font{nullptr, TTF_CloseFont};
    std::vector<SdlOwner<SDL_Texture, SDL_DestroyTexture>> text;
    std::vector<SDL_Rect> text_rectangles;
    bool has_image{};
#endif
};

ReviewWindow::ReviewWindow(WindowOptions options)
    : options_(std::move(options)), window_width_(options_.width), window_height_(options_.height)
{
    validate_options(options_);
#ifdef COMSKIP_DEFAULT_FONT_FILE
    if (options_.font_path.empty()) options_.font_path = COMSKIP_DEFAULT_FONT_FILE;
#endif
}
ReviewWindow::~ReviewWindow() = default;
ReviewWindow::ReviewWindow(ReviewWindow&&) noexcept = default;
ReviewWindow& ReviewWindow::operator=(ReviewWindow&&) noexcept = default;

bool ReviewWindow::available() noexcept
{
#if COMSKIP_BUILD_GUI
    return true;
#else
    return false;
#endif
}
bool ReviewWindow::is_open() const noexcept { return implementation_ != nullptr; }

InputState ReviewWindow::consume_input() noexcept
{
    const auto input = input_;
    input_.key = 0;
    input_.mouse_pressed = false;
    return input;
}

std::uint32_t ReviewWindow::window_id() const noexcept
{
#if COMSKIP_BUILD_GUI
    return implementation_ ? SDL_GetWindowID(implementation_->window.get()) : 0;
#else
    return 0;
#endif
}

void ReviewWindow::open(int width, int height, std::string_view title)
{
    auto options = options_;
    options.width = width;
    options.height = height;
    options.title = title;
    validate_options(options);
    close();
    options_ = std::move(options);
    open();
}

void ReviewWindow::open()
{
    if (is_open()) throw std::logic_error("Review window is already open");
#if COMSKIP_BUILD_GUI
    auto implementation = std::make_unique<Impl>();
    implementation->window.reset(SDL_CreateWindow(options_.title.c_str(), SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, options_.width, options_.height,
        SDL_WINDOW_RESIZABLE | (options_.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN)));
    if (!implementation->window) fail_sdl("Cannot create review window");
    implementation->renderer.reset(SDL_CreateRenderer(implementation->window.get(), -1, SDL_RENDERER_ACCELERATED));
    if (!implementation->renderer)
        implementation->renderer.reset(SDL_CreateRenderer(implementation->window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!implementation->renderer) fail_sdl("Cannot create review renderer");
    implementation->image.reset(SDL_CreateTexture(implementation->renderer.get(), SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, options_.width, options_.height));
    if (!implementation->image) fail_sdl("Cannot create review image texture");
    if (!options_.font_path.empty()) {
        const auto utf8 = options_.font_path.u8string();
        implementation->font.reset(TTF_OpenFont(reinterpret_cast<const char*>(utf8.c_str()), options_.font_size));
        if (!implementation->font)
            throw std::runtime_error(std::string("Cannot open review font: ") + TTF_GetError());
    }
    implementation_ = std::move(implementation);
    input_ = {};
    window_width_ = options_.width;
    window_height_ = options_.height;
    overlay_text_.clear();
    present();
#else
    throw std::runtime_error("Review UI is unavailable; rebuild with COMSKIP_BUILD_GUI=ON");
#endif
}

void ReviewWindow::draw(std::span<const std::uint8_t> rgb, int pitch)
{
    if (!is_open()) throw std::logic_error("Cannot draw to a closed review window");
    if (pitch == 0) pitch = options_.width * 3;
    if (pitch < options_.width * 3) throw std::invalid_argument("Review image pitch is smaller than its RGB row");
    const auto required = static_cast<std::uint64_t>(pitch) * (options_.height - 1) + options_.width * 3;
    if (rgb.size() < required) throw std::invalid_argument("Review image does not contain every RGB row");
#if COMSKIP_BUILD_GUI
    if (SDL_UpdateTexture(implementation_->image.get(), nullptr, rgb.data(), pitch) != 0)
        fail_sdl("Cannot update review image");
    implementation_->has_image = true;
    refresh();
    present();
#endif
}

void ReviewWindow::present()
{
#if COMSKIP_BUILD_GUI
    if (!implementation_) return;
    auto* renderer = implementation_->renderer.get();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (SDL_RenderClear(renderer) != 0) fail_sdl("Cannot clear review window");
    if (implementation_->has_image && SDL_RenderCopy(renderer, implementation_->image.get(), nullptr, nullptr) != 0)
        fail_sdl("Cannot draw review image");
    if (!implementation_->text.empty()) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_Rect background{0, 24, window_width_, std::max(window_height_ - 24, 0)};
        SDL_RenderFillRect(renderer, &background);
        for (std::size_t index = 0; index < implementation_->text.size(); ++index)
            if (SDL_RenderCopy(renderer, implementation_->text[index].get(), nullptr,
                               &implementation_->text_rectangles[index]) != 0)
                fail_sdl("Cannot draw review text");
    }
    SDL_RenderPresent(renderer);
#endif
}

void ReviewWindow::process_event(const Event& event)
{
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, KeyEvent>) {
            input_.shift = value.shift;
            input_.alt = value.alt;
            if (value.key == Key::shift) input_.shift = value.pressed;
            if (value.pressed) input_.key = controller_key(value);
        } else if constexpr (std::is_same_v<T, MouseEvent>) {
            const auto scale = [](int coordinate, int canvas, int window) {
                const auto scaled = static_cast<std::int64_t>(coordinate) * canvas / std::max(window, 1);
                return static_cast<int>(std::clamp<std::int64_t>(scaled, 0, canvas - 1));
            };
            input_.mouse_x = scale(value.x, options_.width, window_width_);
            input_.mouse_y = scale(value.y, options_.height, window_height_);
            if (value.left_button) {
                if (value.kind == MouseEvent::Kind::press) input_.mouse_down = input_.mouse_pressed = true;
                if (value.kind == MouseEvent::Kind::release) input_.mouse_down = false;
                if (value.kind == MouseEvent::Kind::move && input_.mouse_down) input_.mouse_pressed = true;
            }
        } else if constexpr (std::is_same_v<T, ResizeEvent>) {
            if (value.width > 0 && value.height > 0) {
                window_width_ = value.width;
                window_height_ = value.height;
            }
        } else if constexpr (std::is_same_v<T, QuitEvent>) {
            input_.quit_requested = true;
            input_.key = 27;
        }
    }, event);
}

#if COMSKIP_BUILD_GUI
namespace {
bool dispatch_event(ReviewWindow& window, const SDL_Event& event)
{
    const auto id = window.window_id();
    switch (event.type) {
    case SDL_QUIT: window.process_event(QuitEvent{}); return true;
    case SDL_WINDOWEVENT:
        if (event.window.windowID != id) return false;
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) window.process_event(QuitEvent{});
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            window.process_event(ResizeEvent{event.window.data1, event.window.data2});
        return true;
    case SDL_KEYDOWN: case SDL_KEYUP:
        if (event.key.windowID != id) return false;
        window.process_event(KeyEvent{sdl_key(event.key.keysym.sym), event.type == SDL_KEYDOWN,
            (event.key.keysym.mod & KMOD_SHIFT) != 0, (event.key.keysym.mod & KMOD_ALT) != 0});
        return true;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        if (event.button.windowID != id) return false;
        window.process_event(MouseEvent{event.type == SDL_MOUSEBUTTONDOWN ? MouseEvent::Kind::press : MouseEvent::Kind::release,
            event.button.x, event.button.y, event.button.button == SDL_BUTTON_LEFT});
        return true;
    case SDL_MOUSEMOTION:
        if (event.motion.windowID != id) return false;
        window.process_event(MouseEvent{MouseEvent::Kind::move, event.motion.x, event.motion.y});
        return true;
    default: return true;
    }
}
}
#endif

void ReviewWindow::refresh()
{
#if COMSKIP_BUILD_GUI
    if (!is_open()) return;
    std::vector<SDL_Event> other_windows;
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (!dispatch_event(*this, event)) other_windows.push_back(event);
    for (auto& deferred : other_windows)
        if (SDL_PushEvent(&deferred) < 0) fail_sdl("Cannot retain another review window's input");
    present();
#endif
}

void ReviewWindow::wait()
{
    if (!is_open()) throw std::logic_error("Cannot wait on a closed review window");
#if COMSKIP_BUILD_GUI
    SDL_Event event;
    if (SDL_WaitEvent(&event) == 0) fail_sdl("Cannot wait for review input");
    if (!dispatch_event(*this, event) && SDL_PushEvent(&event) < 0)
        fail_sdl("Cannot retain another review window's input");
    refresh();
#endif
}

void ReviewWindow::show_details(std::string_view text)
{
    if (!is_open()) throw std::logic_error("Cannot display text in a closed review window");
#if COMSKIP_BUILD_GUI
    if (!implementation_->font)
        throw std::runtime_error("Review text requires a font; configure WindowOptions::font_path");
    std::vector<SdlOwner<SDL_Texture, SDL_DestroyTexture>> textures;
    std::vector<SDL_Rect> rectangles;
    std::size_t offset = 0;
    int y = 30;
    while (offset <= text.size()) {
        auto end = text.find('\n', offset);
        if (end == std::string_view::npos) end = text.size();
        const std::string line(text.substr(offset, end - offset));
        if (!line.empty()) {
            SdlOwner<SDL_Surface, SDL_FreeSurface> surface(TTF_RenderUTF8_Blended_Wrapped(implementation_->font.get(),
                line.c_str(), SDL_Color{255, 255, 255, 255}, static_cast<Uint32>(std::max(window_width_ - 16, 1))), SDL_FreeSurface);
            if (!surface) throw std::runtime_error(std::string("Cannot render review text: ") + TTF_GetError());
            SdlOwner<SDL_Texture, SDL_DestroyTexture> texture(SDL_CreateTextureFromSurface(implementation_->renderer.get(), surface.get()), SDL_DestroyTexture);
            if (!texture) fail_sdl("Cannot create review text texture");
            rectangles.push_back(SDL_Rect{8, y, surface->w, surface->h});
            textures.push_back(std::move(texture));
            y += surface->h + 2;
        } else y += TTF_FontLineSkip(implementation_->font.get());
        if (end == text.size()) break;
        offset = end + 1;
    }
    implementation_->text = std::move(textures);
    implementation_->text_rectangles = std::move(rectangles);
    overlay_text_ = text;
    present();
#endif
}

void ReviewWindow::show_help(std::span<const std::string_view> lines)
{
    std::string text;
    bool first = true;
    for (const auto line : lines) {
        if (!first) text += '\n';
        text += line;
        first = false;
    }
    show_details(text);
}

void ReviewWindow::clear_text()
{
    overlay_text_.clear();
#if COMSKIP_BUILD_GUI
    if (implementation_) {
        implementation_->text.clear();
        implementation_->text_rectangles.clear();
        present();
    }
#endif
}

void ReviewWindow::close() noexcept
{
    implementation_.reset();
    input_ = {};
    overlay_text_.clear();
}
}
