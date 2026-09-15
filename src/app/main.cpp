#include "arguments.h"
#include "recording_context.h"
#include <memory>
#include "exit_requested.h"
#include <cstdio>
#include <exception>

int comskip_main(RecordingContext& context, int argc, char** argv);
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        comskip::Arguments arguments(argc, argv);
        auto context = std::make_unique<RecordingContext>();
        return comskip_main(*context, arguments.size(), arguments.data());
    } catch (const comskip::ExitRequested& request) {
        return request.status();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Comskip: %s\n", error.what());
        return 2;
    }
}
#else
int main(int argc, char** argv) {
    try { auto context = std::make_unique<RecordingContext>(); return comskip_main(*context, argc, argv); }
    catch (const comskip::ExitRequested& request) { return request.status(); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Comskip: %s\n", error.what());
        return 2;
    }
}
#endif
