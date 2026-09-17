#include "platform/utf8_paths.h"
#include "config/legacy_settings.h"
#include "media/decoder.h"
#include "ui/executable_mode.h"
#include "exit_requested.h"
#include "app/build_info.h"
#include "app/comskip.h"
#include "app/debug.h"
#include "app/recording_context.h"
#include "checked_format.h"
#include "detection/detection_methods.h"
#include "detection/logo_detection.h"
#include "detection/scene_analysis.h"
#include "output/diagnostics.h"
#include "output/checked_file.h"
#include "platform/platform.h"
#include "app/csv_input.h"
#include "ui/review.h"
#include "translator.h"
#include "diagnostic_render.h"
#include "logo_search_time.h"
#include "arguments.h"
#include "command_line_value.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

#include <argtable2.h>

namespace {
using comskip::platform::path_from_utf8;
using comskip::platform::path_to_utf8;
using comskip::detection::DetectionMethod;
void print_argument_errors(FILE& output, const struct arg_end& errors,
                           const comskip::localization::Translator& translator) {
    // Argtable remains responsible for parsing and validation. Its public error
    // records supply the untranslated option/value; catalogs supply UI text.
    for (int i = 0; i < errors.count; ++i) {
        const auto& header = *static_cast<const arg_hdr*>(errors.parent[i]);
        const std::string_view value = errors.argval[i] ? errors.argval[i] : "";
        if (header.flag & ARG_TERMINATOR) {
            switch (errors.error[i]) {
            case ARG_ELIMIT: fputs(translator.text("cli_too_many_errors"), &output); break;
            case ARG_EMALLOC: fputs(translator.format("cli_insufficient_memory", "Comskip").c_str(), &output); break;
            case ARG_ENOMATCH: fputs(translator.format("cli_unexpected_argument", value).c_str(), &output); break;
            case ARG_EMISSARG: fputs(translator.format("cli_missing_value", value).c_str(), &output); break;
            case ARG_ELONGOPT: fputs(translator.format("cli_invalid_option", value).c_str(), &output); break;
            default:
                fputs(translator.format("cli_invalid_option",
                      std::string("-") + static_cast<char>(errors.error[i])).c_str(), &output);
            }
        } else {
            const std::string option = header.longopts ? std::string("--") + header.longopts :
                header.shortopts ? std::string("-") + header.shortopts :
                header.glossary ? header.glossary : "";
            fputs(translator.format("cli_invalid_argument", option, value).c_str(), &output);
        }
    }
}
}

std::string intSecondsToStrMinutes(int seconds)
{
    int minutes, hours;
    hours = static_cast<int>(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = static_cast<int>(seconds / 60);
    seconds -= minutes * 60;
    return std::format("{}:{:02}:{:02}", hours, minutes, seconds);
}

std::string dblSecondsToStrMinutes(double seconds)
{
    int minutes, hours;
    hours = static_cast<int>(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = static_cast<int>(seconds / 60);
    seconds -= minutes * 60;
    const auto whole_seconds = static_cast<int>(seconds);
    const auto hundredths = static_cast<int>((seconds - whole_seconds) * 100);
    return std::format("{}:{:02}:{:02}.{:02}", hours, minutes, whole_seconds, hundredths);
}

std::string dblSecondsToStrMinutesFrames(double seconds, double fps)
{
    int minutes, hours;
    hours = static_cast<int>(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = static_cast<int>(seconds / 60);
    seconds -= minutes * 60;
    const auto whole_seconds = static_cast<int>(seconds);
    const auto hundredths = static_cast<int>((seconds - whole_seconds) * 100.0);
    const auto frames = static_cast<int>(hundredths * fps / 100.0);
    return std::format("{}:{:02}:{:02}.{:02}", hours, minutes, whole_seconds, frames);
}



void LoadIniFile(RecordingContext& context)
{
    LoadIniFile(context, context.translator);
}

void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator)
{
    if (!context.state.ini_file.get()) {
        FindIniFile(context);
        if (!context.state.inifilename.empty()) context.state.ini_file = comskip::platform::open_file_owned(context.state.inifilename, "r");
    }
    try {
        context.state.ini_text = comskip::config::defaults().serialize();
        if (context.state.ini_file.get()) {
            std::string data;
            char buffer[4096];
            std::size_t count;
            while ((count = fread(buffer, 1, sizeof buffer, context.state.ini_file.get())) != 0) data.append(buffer, count);
            bool failed = ferror(context.state.ini_file.get()) != 0;
            context.state.ini_file.reset();
            if (failed) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_read_recording_ini_file, {context.state.inifilename});
            comskip::config::Ini ini(data);
            context.settings = comskip::config::load_settings(ini, context.settings);
            context.state.ini_text += ini.serialize();
            fputs(translator.format("using_settings", context.state.inifilename).c_str(), stdout);
        }
        for (const char* file : {context.settings.cutscenefile1.c_str(), context.settings.cutscenefile2.c_str(), context.settings.cutscenefile3.c_str(), context.settings.cutscenefile4.c_str(),
                                 context.settings.cutscenefile5.c_str(), context.settings.cutscenefile6.c_str(), context.settings.cutscenefile7.c_str(), context.settings.cutscenefile8.c_str()})
            if (*file) LoadCutScene(context, file);
    } catch (const std::exception& error) {
        fputs(translator.format("invalid_configuration", comskip::localization::render_exception(error,translator)).c_str(), stderr);
        comskip::request_exit(1);
    }
    context.settings.giveUpOnLogoSearch = comskip::config::adjusted_logo_search_seconds(
        context.settings.giveUpOnLogoSearch, context.settings.added_recording);
}


void LoadSettings(RecordingContext& context, int argc, char ** argv, const comskip::localization::Translator& translator)
{
    std::string start_timestamp;
    bool has_local_time = false;
    comskip::platform::FilePtr logo_file;
    comskip::platform::FilePtr log_file;
    comskip::platform::FilePtr test_file;
    int					i = 0;
//	int					play_nice_start = -1;
//	int					play_nice_end = -1;
    time_t				ltime;
    struct tm now{};
    int					mil_time;
    struct arg_lit*		cl_playnice				= arg_lit0("n", "playnice", translator.text("option_0"));
    struct arg_lit*		cl_output_zp_cutlist	= arg_lit0(NULL, "zpcut", translator.text("option_1"));
    struct arg_lit*		cl_output_zp_chapter	= arg_lit0(NULL, "zpchapter", translator.text("option_2"));
    struct arg_lit*		cl_output_scf			= arg_lit0(NULL, "scf", translator.text("option_3"));
    struct arg_lit*		cl_output_vredo			= arg_lit0(NULL, "videoredo", translator.text("option_4"));
    struct arg_lit*		cl_output_vredo3		= arg_lit0(NULL, "videoredo3", translator.text("option_5"));
    struct arg_lit*		cl_output_csv			= arg_lit0(NULL, "csvout", translator.text("option_6"));
    struct arg_lit*		cl_output_training		= arg_lit0(NULL, "quality", translator.text("option_7"));
    struct arg_lit*		cl_output_plist	= arg_lit0(NULL, "plist", translator.text("option_8"));
    struct arg_int*		cl_detectmethod			= arg_intn("d", "detectmethod", NULL, 0, 1, translator.text("option_9"));
//	struct arg_int*		cl_pid					= arg_intn("p", "pid", NULL, 0, 1, "The PID of the video in the TS");
    struct arg_str*		cl_pid					= arg_strn("p", "pid", NULL, 0, 1, translator.text("option_10"));
    struct arg_int*		cl_dump					= arg_intn("u", "dump", NULL, 0, 1, translator.text("option_11"));
    struct arg_lit*		cl_ts					= arg_lit0("t", "ts", translator.text("option_12"));
    struct arg_lit*		cl_help					= arg_lit0("h", "help", translator.text("option_13"));
    struct arg_lit*		cl_show					= arg_lit0("s", "play", translator.text("option_14"));
    struct arg_lit*		cl_timing				= arg_lit0(NULL, "timing", translator.text("option_15"));
    struct arg_lit*		cl_debugwindow			= arg_lit0("w", "debugwindow", translator.text("option_16"));
    struct arg_lit*		cl_quiet				= arg_lit0("q", "quiet", translator.text("option_17"));
    struct arg_lit*		cl_demux				= arg_lit0("m", "demux", translator.text("option_18"));
    struct arg_lit*		cl_hwassist				= arg_lit0(NULL, "hwassist", translator.text("option_19"));
    struct arg_lit*		cl_use_cuvid			= arg_lit0(NULL, "cuvid", translator.text("option_20"));
    struct arg_lit*		cl_use_vdpau			= arg_lit0(NULL, "vdpau", translator.text("option_21"));
    struct arg_lit*		cl_use_dxva2			= arg_lit0(NULL, "dxva2", translator.text("option_22"));
    struct arg_lit*		cl_use_qsv				= arg_lit0(NULL, "qsv", translator.text("option_23"));
    struct arg_lit*		cl_list_decoders		= arg_lit0(NULL, "decoders", translator.text("option_24"));
    struct arg_int*		cl_threads				= arg_int0(NULL, "threads", "<int>", translator.text("option_25"));
    struct arg_int*		cl_verbose				= arg_intn("v", "verbose", NULL, 0, 1, translator.text("option_26"));
    struct arg_file*	cl_ini					= arg_filen(NULL, "ini", NULL, 0, 1, translator.text("option_27"));
    struct arg_file*	cl_logo					= arg_filen(NULL, "logo", NULL, 0, 1, translator.text("option_28"));
    struct arg_file*	cl_cut					= arg_filen(NULL, "cut", NULL, 0, 1, translator.text("option_29"));
    struct arg_file*	cl_work					= arg_filen(NULL, "output", NULL, 0, 1, translator.text("option_30"));
    struct arg_file*	cl_work_fname		= arg_filen(NULL, "output-filename", NULL, 0, 1, translator.text("option_31"));
    struct arg_int*	cl_selftest					= arg_intn(NULL, "selftest", NULL, 0, 1, translator.text("option_32"));
    struct arg_file*	in						= arg_filen(NULL, NULL, NULL, 1, 1, translator.text("option_33"));
    struct arg_file*	out						= arg_filen(NULL, NULL, NULL, 0, 1, translator.text("option_34"));
    struct arg_end*		end						= arg_end(20);
    struct arg_str* cl_language = arg_str0(NULL, "language", "<en|es>", translator.text("option_language"));
    void*				argtable[] =
    {
        cl_help,
        cl_language,
        cl_debugwindow,
        cl_playnice,
        cl_output_zp_cutlist,
        cl_output_zp_chapter,
        cl_output_scf,
        cl_output_vredo,
        cl_output_vredo3,
        cl_output_csv,
        cl_output_training,
        cl_output_plist,
        cl_demux,
        cl_hwassist,
        cl_use_cuvid,
        cl_use_vdpau,
        cl_use_dxva2,
        cl_use_qsv,
        cl_list_decoders,
        cl_threads,
        cl_pid,
        cl_ts,
        cl_detectmethod,
        cl_verbose,
        cl_dump,
        cl_show,
        cl_timing,
        cl_quiet,
        cl_ini,
        cl_logo,
        cl_cut,
        cl_work,
        cl_work_fname,
        cl_selftest,
        in,
        out,
        end
    };
    struct ArgTableOwner {
        void** entries;
        std::size_t count;
        ~ArgTableOwner() { arg_freetable(entries, count); }
    } parser_owner{argtable, std::size(argtable)};
    int					nerrors;

    // Print out the command line parameters
    fputs(translator.text("commandline"), stdout);
    for (i = 0; i < argc; i++)
    {
        if (strchr(argv[i], ' '))
        {
            std::cout << "\t\"" << argv[i] << "\"\n";
        }
        else
        {
            std::cout << '\t' << argv[i] << '\n';
        }
    }
    std::cout << "\n\n";

    context.state.argument = comskip::snapshot_arguments(argc, argv);

    if (argc <= 1)
    {

        if (comskip::ui::gui_executable(argv[0]))
            context.settings.output_debugwindow = true;
        if (context.settings.output_debugwindow)
        {
// This is a trick to ask for a input filename when no argument has been given.
//				while (mpegfilename[0] == 0)
//					ReviewResult();
//				argc++;
//				strcpy(argument[1], mpegfilename);
        }
    }



    // verify the argtable[] entries were allocated sucessfully
    if (arg_nullcheck(argtable) != 0)
    {

        // NULL entries were detected, some allocations must have failed
        Debug(context, 0, translator.format("cli_insufficient_memory", context.state.progname));
        return;
    }

    nerrors = arg_parse(argc, argv, argtable);
    if (cl_list_decoders->count)
    {
        list_codecs(translator);
        comskip::request_exit(2);
    }
    if (cl_help->count)
    {
        fputs(translator.text("usage"), stdout);
        arg_print_syntaxv(stdout, argtable, "\n\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        fputs(translator.text("methods"), stdout);
        fputs(translator.format("method_black", static_cast<int>(DetectionMethod::black_frame)).c_str(), stdout);
        fputs(translator.format("method_logo", static_cast<int>(DetectionMethod::logo)).c_str(), stdout);
        fputs(translator.format("method_scene", static_cast<int>(DetectionMethod::scene_change)).c_str(), stdout);
        fputs(translator.format("method_resolution", static_cast<int>(DetectionMethod::resolution_change)).c_str(), stdout);
        fputs(translator.format("method_captions", static_cast<int>(DetectionMethod::captions)).c_str(), stdout);
        fputs(translator.format("method_aspect", static_cast<int>(DetectionMethod::aspect_ratio)).c_str(), stdout);
        fputs(translator.format("method_silence", static_cast<int>(DetectionMethod::silence)).c_str(), stdout);
        fputs(translator.format("method_cutscenes", static_cast<int>(DetectionMethod::cutscene)).c_str(), stdout);
        fputs(translator.text("all_methods"), stdout);
        comskip::request_exit(2);
    }

    if (nerrors)
    {
        fputs(translator.text("usage"), stdout);
        arg_print_syntaxv(stdout, argtable, "\n\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        fputs(translator.text("available_methods"), stdout);
        fputs(translator.format("method_black", static_cast<int>(DetectionMethod::black_frame)).c_str(), stdout);
        fputs(translator.format("method_logo", static_cast<int>(DetectionMethod::logo)).c_str(), stdout);
        fputs(translator.format("method_scene", static_cast<int>(DetectionMethod::scene_change)).c_str(), stdout);
        fputs(translator.format("method_resolution", static_cast<int>(DetectionMethod::resolution_change)).c_str(), stdout);
        fputs(translator.format("method_captions", static_cast<int>(DetectionMethod::captions)).c_str(), stdout);
        fputs(translator.format("method_aspect", static_cast<int>(DetectionMethod::aspect_ratio)).c_str(), stdout);
        fputs(translator.format("method_silence", static_cast<int>(DetectionMethod::silence)).c_str(), stdout);
        fputs(translator.format("method_cutscenes", static_cast<int>(DetectionMethod::cutscene)).c_str(), stdout);
        fputs(translator.text("all_methods"), stdout);
        fputs(translator.text("errors"), stdout);
        print_argument_errors(*stdout, *end, translator);
        comskip::request_exit(2);
    }

    if (strcmp(in->extension[0], ".csv") != 0 && strcmp(in->extension[0], ".txt") != 0)
    {
        context.state.mpegfilename = in->filename[0];



        context.state.inbasename = path_to_utf8(path_from_utf8(in->filename[0]).replace_extension());
        context.state.shortbasename = path_to_utf8(path_from_utf8(context.state.inbasename).filename());

 //       comskip::checked_format(mpegfilename, "%.*s.txt", (int)strlen(inbasename), inbasename);

        context.state.inifilename = path_to_utf8(path_from_utf8(context.state.inbasename).parent_path() / "comskip.ini");
    }
    else if (strcmp(in->extension[0], ".csv") == 0)
    {
        context.state.loadingCSV = true;
        context.state.in_file = comskip::platform::open_file_owned(in->filename[0], "r");
        fputs(translator.format("array_open", in->filename[0]).c_str(), stdout);
        if (!context.state.in_file.get())
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            comskip::request_exit(4);
        }

        context.state.inbasename = path_to_utf8(path_from_utf8(in->filename[0]).replace_extension());
        context.state.mpegfilename = std::string(context.state.inbasename) + ".mpg";
        test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".ts";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".tp";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".dvr-ms";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".wtv";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".mp4";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".mkv";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename.clear();
        }
        else
        {
            test_file.reset();
        }


        context.state.shortbasename = path_to_utf8(path_from_utf8(context.state.inbasename).filename());
        context.state.inifilename = path_to_utf8(path_from_utf8(context.state.inbasename).parent_path() / "comskip.ini");
        if (context.state.mpegfilename.empty()) context.state.mpegfilename = std::string(context.state.inbasename) + ".mpg";
    }
    else if (strcmp(in->extension[0], ".txt") == 0)
    {
        context.state.loadingTXT = true;
        context.settings.output_default = false;
        context.state.in_file = comskip::platform::open_file_owned(in->filename[0], "r");
        fputs(translator.format("review_open", in->filename[0]).c_str(), stdout);
        if (!context.state.in_file.get())
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            comskip::request_exit(4);
        }
        context.state.in_file.reset();
        context.state.in_file.reset();

        context.state.inbasename = path_to_utf8(path_from_utf8(in->filename[0]).replace_extension());
        context.state.mpegfilename = std::string(context.state.inbasename) + ".mpg";
        test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".ts";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".tp";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".dvr-ms";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".wtv";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".mp4";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename = std::string(context.state.inbasename) + ".mkv";
            test_file = comskip::platform::open_file_owned(context.state.mpegfilename, "rb");
        }
        if (!test_file)
        {
            context.state.mpegfilename.clear();
        }
        else
        {
            test_file.reset();
        }

        context.state.shortbasename = path_to_utf8(path_from_utf8(context.state.inbasename).filename());
        context.state.inifilename = path_to_utf8(path_from_utf8(context.state.inbasename).parent_path() / "comskip.ini");
//		comskip::checked_format(mpegfilename, "%s.mpg", inbasename);
    }
    else
    {
        fputs(translator.format("unsupported_input", in->extension[0]).c_str(), stdout);
        comskip::request_exit(5);
    }
    if (cl_ini->count)
    {
        context.state.inifilename = std::string(cl_ini->filename[0]);
        fputs(translator.format("setting_ini", context.state.inifilename).c_str(), stdout);
    }
    context.state.ini_file = comskip::platform::open_file_owned(context.state.inifilename, "r");

    if (cl_work_fname->count)
    {
        context.state.shortbasename = std::string(cl_work_fname->filename[0]);
    }

    if (cl_work->count)
    {
        context.state.outputdirname = std::string(cl_work->filename[0]);
        context.state.workbasename = path_to_utf8(path_from_utf8(context.state.outputdirname) / path_from_utf8(context.state.shortbasename));
        context.state.outbasename = context.state.workbasename;
    }
    else
    {
        context.state.outputdirname.clear();
        context.state.workbasename = context.state.inbasename;
    }


    if (out->count)
    {
        context.state.outputdirname = std::string(out->filename[0]);
        context.state.outbasename = path_to_utf8(path_from_utf8(context.state.outputdirname) / path_from_utf8(context.state.shortbasename));
    }
    else
    {
        context.state.outputdirname.clear();
        context.state.outbasename = context.state.inbasename;
    }

    if (cl_work->count && !out->count)   // --output also sets the output file location if not specified as 2nd argument.
    {
        context.state.outbasename = context.state.workbasename;
    }


    context.state.logofilename = std::string(context.state.workbasename) + ".logo.txt";
    context.state.logfilename = std::string(context.state.workbasename) + ".log";
    context.state.filename = std::string(context.state.outbasename) + ".txt";
    if (strcmp(context.state.HomeDir.c_str(), ".") == 0)
    {
        if (!context.state.ini_file.get())
        {
            context.state.inifilename = "comskip.ini";
    context.state.ini_file = comskip::platform::open_file_owned(context.state.inifilename, "r");
        }
        context.state.exefilename = "comskip.exe";
        context.state.dictfilename = "comskip.dictionary";
    }
    else
    {
        if (!context.state.ini_file.get())
        {
            context.state.inifilename = path_to_utf8(path_from_utf8(context.state.HomeDir) / "comskip.ini");
    context.state.ini_file = comskip::platform::open_file_owned(context.state.inifilename, "r");
        }
        context.state.exefilename = path_to_utf8(path_from_utf8(context.state.HomeDir) / "comskip.exe");
        context.state.dictfilename = path_to_utf8(path_from_utf8(context.state.HomeDir) / "comskip.dictionary");
    }

    if (cl_cut->count)
    {
        fputs(translator.format("loading_cut", cl_cut->filename[0]).c_str(), stdout);
        LoadCutScene(context, cl_cut->filename[0]);
    }

    if (cl_logo->count)
    {
        context.state.logofilename = std::string(cl_logo->filename[0]);
        fputs(translator.format("setting_logo", context.state.logofilename).c_str(), stdout);
    }



    //	if (!loadingTXT)
    LoadIniFile(context, translator);

//	live_tv = true;

    time(&ltime);
    has_local_time = comskip::platform::local_time(ltime, now);
    start_timestamp = comskip::platform::time_string(ltime);
    mil_time = has_local_time ? (now.tm_hour * 100) + now.tm_min : 0;
    if ((context.settings.play_nice_start > -1) && (context.settings.play_nice_end > -1))
    {
        if (context.settings.play_nice_start > context.settings.play_nice_end)
        {
            if ((mil_time >= context.settings.play_nice_start) || (mil_time <= context.settings.play_nice_end)) context.state.play_nice = true;
        }
        else
        {
            if ((mil_time >= context.settings.play_nice_start) && (mil_time <= context.settings.play_nice_end)) context.state.play_nice = true;
        }
    }

    if (cl_verbose->count)
    {
        context.settings.verbose = cl_verbose->ival[0];
        fputs(translator.format("setting_verbose", context.settings.verbose).c_str(), stdout);
    }

    if (cl_selftest->count)
    {
        context.state.selftest = cl_selftest->ival[0];
        fputs(translator.format("setting_selftest", context.state.selftest).c_str(), stdout);
    }

    if (cl_debugwindow->count || context.state.loadingTXT)
    {
        context.settings.output_debugwindow = true;
    }
    if (cl_timing->count)
    {
        context.settings.output_timing = true;
    }
    if (cl_show->count)
    {
        context.state.subsample_video = 0;
        context.settings.output_debugwindow = true;
    }

    if (cl_quiet->count)
    {
        context.state.output_console = false;
    }


    if (comskip::ui::gui_executable(argv[0]))
        context.settings.output_debugwindow = true;

    if (cl_demux->count)
    {
        context.settings.output_demux = true;
    }

    if (cl_hwassist->count)
    {
        context.settings.hardware_decode = 1;
    }
    if (cl_use_cuvid->count)
    {
        fputs(translator.text("enable_cuvid"), stdout);
        context.state.use_cuvid = 1;
    }
    if (cl_use_vdpau->count)
    {
        fputs(translator.text("enable_vdpau"), stdout);
        context.state.use_vdpau = 1;
    }

    if (cl_use_dxva2->count)
    {
        fputs(translator.text("enable_dxva2"), stdout);
        context.state.use_dxva2 = 1;
    }

    if (cl_use_qsv->count)
    {
        fputs(translator.text("enable_qsv"), stdout);
        context.state.use_qsv = 1;
    }

    if (cl_threads->count)
    {
        context.settings.thread_count = cl_threads->ival[0];
    }

    if (!context.state.loadingTXT && !context.settings.useExistingLogoFile && cl_logo->count==0)
    {
        logo_file = comskip::platform::open_file_owned(context.state.logofilename, "r");
        if(logo_file)
        {
            logo_file.reset();
            myremove(context.state.logofilename.c_str());
        }
    }

    if (cl_output_csv->count)
    {
        context.settings.output_framearray = true;
    }

    if (cl_output_training->count)
    {
        context.settings.output_training = true;
    }



    if (context.settings.verbose)
    {
        logo_file = comskip::platform::open_file_owned(context.state.logofilename, "r");
        if (context.state.loadingTXT)
        {
            // Do nothing to the log file
            context.settings.verbose = 0;
        }
        else if (context.state.loadingCSV)
        {
            log_file = comskip::platform::open_file_owned(context.state.logfilename, "w");
            if (log_file) {
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Generated using %s %s\n", comskip::build::distribution_variant.data(), package_string);
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Loading comskip csv file - %s\n", in->filename[0]);
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Time at start of run:\n%s", start_timestamp.c_str());
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                log_file.reset();
            }
        }
        else if (logo_file)
        {
            logo_file.reset();
            log_file = comskip::platform::open_file_owned(context.state.logfilename, "a+");
            if (log_file) {
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Starting second pass using %s\n", context.state.logofilename.c_str());
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Time at start of second run:\n%s", start_timestamp.c_str());
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                log_file.reset();
            }
        }
        else
        {
            log_file = comskip::platform::open_file_owned(context.state.logfilename, "w");
            if (log_file) {
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Generated using %s %s\n", comskip::build::distribution_variant.data(), package_string);
                comskip::output::checked_fprintf(*log_file, context.state.logfilename,
                    "Time at start of run:\n%s", start_timestamp.c_str());
                comskip::output::checked_fprintf(*log_file, context.state.logfilename, "################################################################\n");
                log_file.reset();
            }
        }
    }

    if (cl_playnice->count)
    {
        context.state.play_nice = true;
        Debug(context, 1, translator.text("cli_playnice"));
    }

    if (cl_detectmethod->count)
    {
        context.settings.commDetectMethod = cl_detectmethod->ival[0];
        fputs(translator.format("setting_methods", context.settings.commDetectMethod).c_str(), stdout);
    }

    if (cl_dump->count)
    {
        context.settings.cutsceneno = cl_dump->ival[0];
        fputs(translator.format("setting_dump", context.settings.cutsceneno).c_str(), stdout);
    }

    if (cl_ts->count)
    {
        context.state.demux_pid = 1;
        fputs(translator.text("auto_pid"), stdout);
    }

    if (cl_pid->count)
    {
        const auto pid=comskip::config::parse_transport_stream_pid(cl_pid->sval[0]);
        if (!pid) {
            fputs(translator.format("cli_invalid_argument", "--pid", cl_pid->sval[0]).c_str(), stderr);
            comskip::request_exit(1);
        }
        context.state.demux_pid=*pid;
        fputs(translator.format("setting_pid", std::format("{:x}", context.state.demux_pid)).c_str(), stdout);
    }



    Debug(context, 9, translator.format("settings_input_files", context.state.mpegfilename,
        context.state.exefilename, context.state.logofilename, context.state.inifilename));
    Debug(context, 1, translator.text("settings_detection_methods"));
    i = 0;
    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::black_frame))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_black", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::logo))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_logo", i,
            context.settings.giveUpOnLogoSearch));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::cutscene))
    {
//		commDetectMethod &= ~SCENE_CHANGE;
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::scene_change))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_scene_change", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::resolution_change))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_resolution_change", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::captions))
    {
        i++;
        context.state.processCC = true;
        Debug(context, 1, translator.format("settings_method_closed_captions", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::aspect_ratio))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_aspect_ratio", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::silence))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_silence", i));
    }

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, DetectionMethod::cutscene))
    {
        i++;
        Debug(context, 1, translator.format("settings_method_cutscenes", i));
    }


    Debug(context, 1, "\n");
    if (context.settings.play_nice_start || context.settings.play_nice_end)
    {
        Debug(context,
            1,
            "%s", translator.format("cli_throttle_schedule",
                std::format("{:04}", context.settings.play_nice_start),
                std::format("{:04}", context.settings.play_nice_end),
                std::format("{:04}", mil_time)).c_str()
        );
        if (context.state.play_nice)
        {
            Debug(context, 1, translator.text("cli_running_slowly"));
        }
        else
        {
            Debug(context, 1, translator.text("cli_full_speed"));
        }
    }

    Debug(context, 10, translator.text("settings_heading"));
    Debug(context, 10, "%s\n", context.state.ini_text.c_str());
    context.state.out_filename = std::string(context.state.outbasename) + ".txt";


    if (!context.state.loadingTXT)
    {
        logo_file = comskip::platform::open_file_owned(context.state.logofilename, "r+");
        if (logo_file)
        {
            Debug(context, 1, translator.text("cli_logo_exists"));
            logo_file.reset();
            LoadLogoMaskData(context);
        }
    }

    context.state.out_file.reset();
    context.state.edl_file.reset();
    context.state.live_file.reset();
    context.state.edlp_file.reset();
    context.state.tuning_file.reset();
    context.state.training_file.reset();

    if (cl_output_plist->count)
        context.settings.output_plist_cutlist = true;
    if (cl_output_zp_cutlist->count)
        context.settings.output_zoomplayer_cutlist = true;
    if (cl_output_zp_chapter->count)
        context.settings.output_zoomplayer_chapter = true;
    if (cl_output_scf->count)
        context.settings.output_scf = true;
    if (cl_output_vredo->count)
        context.settings.output_videoredo = true;
    if (cl_output_vredo3->count)
    {
        context.settings.output_videoredo3 = true;
        context.settings.output_videoredo = false;
    }
    if (cl_output_plist->count)
        context.settings.output_plist_cutlist = true;

    if (context.settings.output_default && ! context.state.loadingTXT)
    {
        if(!context.state.isSecondPass)
        {
            context.state.out_file = comskip::platform::open_file_owned(context.state.out_filename, "w");
            if (!context.state.out_file.get())
            {
                fputs(translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
                comskip::request_exit(6);
            }
            else
            {
                context.settings.output_default = true;
                context.state.out_file.reset();
            }
        }
    }

//	max_commercialbreak *= fps;
//	min_commercialbreak *= fps;
//	max_commercial_size *= fps;
//	min_commercial_size *= fps;

    if (context.state.loadingTXT)
    {
        context.state.frame_count = InputReffer(context, ".txt", true);
        if (context.state.frame_count < 0)
        {
            fputs(translator.text("incompatible_txt"), stdout);
            comskip::request_exit(2);
        }
        context.state.framearray = false;
        fputs(translator.text("close_window"), stdout);
        context.settings.output_debugwindow = true;
        ReviewResult(context);
//		in_file = NULL;
    }

    if (!context.state.loadingTXT && (context.settings.output_srt || context.settings.output_smi))
    {
#ifdef PROCESS_CC
        const auto basename = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(context.state.outbasename.c_str())));
        context.captions = std::make_unique<comskip::media::CaptionSession>(
            comskip::media::CaptionOutputOptions{basename, context.settings.output_srt, context.settings.output_smi});
#endif
    }


    if (context.state.loadingCSV)
    {
        context.settings.output_framearray = false;
        ProcessCSV(context, std::move(context.state.in_file));
        context.settings.output_debugwindow = false;
    }


}
