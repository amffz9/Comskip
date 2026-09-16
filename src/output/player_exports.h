#pragma once
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <span>

namespace comskip::output {
using PlayerSeconds = std::chrono::duration<double>;
enum class PlayerBoundary { commercial_start, commercial_end };
struct PlayerInterval { PlayerSeconds start, end; };
struct PlayerChapterMark { PlayerSeconds at; std::uint32_t number; PlayerBoundary boundary; };
struct ScfFrameMark { std::int64_t frame; std::uint32_t number; PlayerBoundary boundary; };
struct ScfOptions { int rounded_fps; };
void write_zoomplayer_chapters(std::ostream&, std::span<const PlayerChapterMark>, bool initial_show);
void write_zoomplayer_cuts(std::ostream&, std::span<const PlayerInterval>);
void write_scf(std::ostream&, std::span<const ScfFrameMark>, ScfOptions);
void write_ipod_chapters(std::ostream&, std::span<const PlayerChapterMark>);
void write_bsplayer(std::ostream&, std::span<const PlayerInterval>);
}
