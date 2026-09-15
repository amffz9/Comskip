#include "arguments.h"
#include <cstdio>
#include <exception>

int comskip_main(int argc, char** argv);
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        comskip::Arguments arguments(argc, argv);
        return comskip_main(arguments.size(), arguments.data());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Comskip: %s\n", error.what());
        return 2;
    }
}
#else
int main(int argc, char** argv) {
    try { return comskip_main(argc, argv); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Comskip: %s\n", error.what());
        return 2;
    }
}
#endif
