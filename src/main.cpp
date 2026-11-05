// SPDX-License-Identifier: BSD-3-Clause
#include "photoswindow.h"
#include "glyphs.h"

#include "base/i18n.h"
#include "ui/application.h"

#include <cstdio>
#include <filesystem>
#include <string>

#ifndef PHOTOS_VERSION
#define PHOTOS_VERSION "1.0"
#endif

using namespace cutefish;

static void printHelp(const char* program)
{
    std::printf("Usage: %s [options] [FOLDER | IMAGE ...]\n"
                "Photos for Cutefish — a simple photo viewer\n\n"
                "Options:\n"
                "  -h, --help     Displays help on commandline options.\n"
                "  -v, --version  Displays version information.\n\n"
                "Arguments:\n"
                "  folder         The folder of pictures to browse.\n"
                "  image          An image file to open, with its folder as the library.\n",
                program);
}

int main(int argc, char** argv)
{
    std::string argument;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "-h" || option == "--help" || option == "-?") {
            printHelp(argv[0]);
            return 0;
        }
        if (option == "-v" || option == "--version") {
            std::printf("cutefish-photos %s\n", PHOTOS_VERSION);
            return 0;
        }
        if (option.starts_with("-")) {
            std::fprintf(stderr, "Unknown option '%s'.\n", option.c_str());
            return 1;
        }
        if (argument.empty())
            argument = option;
    }

    ui::Application app(argc, argv);
    app.setApplicationId("cutefish-photos");
    i18n::setDomain("cutefish-photos");
    photos::registerPhotoGlyphs();

    photos::PhotosWindow window;
    window.openArgument(argument);
    window.window().show();
    return app.run();
}
