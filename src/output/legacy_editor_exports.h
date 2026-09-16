#pragma once
#include <chrono>
#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>
namespace comskip::output {
using EditorSeconds=std::chrono::duration<double>;
struct EditorInterval {EditorSeconds start,end;};
struct EditorScene {int number; EditorSeconds at;};
struct EditorStreamIds {int video,audio,subtitle;};
struct VideoRedo2Options {std::string_view filename; bool h264; std::optional<EditorStreamIds> streams;};
void write_vdr(std::ostream&,std::span<const EditorInterval>,double fps);
void write_videoredo2(std::ostream&,VideoRedo2Options,std::span<const EditorInterval>,std::span<const EditorScene>);
}
