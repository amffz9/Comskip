#include "exit_requested.h"
#include "legacy_detection.h"
#include "checked_format.h"
#include "translator.h"
#include "arguments.h"

double FindNumber(RecordingContext& context, char* data, const char* key, double fallback)
{
    try {
        std::string name(key);
        if (name.ends_with('=')) name.pop_back();
        std::istringstream input(data ? data : "");
        std::string line, metadata;
        while (std::getline(input, line))
            if (line.find('=') != std::string::npos) metadata += line + "\n";
        comskip::config::Ini ini(metadata);
        return ini.find(name) ? ini.number<double>(name) : fallback;
    } catch (const std::exception& error) {
        Debug(context, 0, "Invalid logo metadata: %s\n", error.what());
        return fallback;
    }
}

char* intSecondsToStrMinutes(RecordingContext& context, int seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(context.state.tempString, "%i:%.2i:%.2i", hours, minutes, seconds);
    return (context.state.tempString);
}

char* dblSecondsToStrMinutes(RecordingContext& context, double seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(context.state.tempString, "%0i:%.2i:%.2d.%.2d", hours, minutes, (int)seconds, (int)((seconds - (int)(seconds))*100) );

    return (context.state.tempString);
}

char* dblSecondsToStrMinutesFrames(RecordingContext& context, double seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(context.state.tempString, "%0i:%.2i:%.2d.%.2d", hours, minutes, (int)seconds, (int)(((int)((seconds - (int)(seconds))*100.0)) * context.settings.fps / 100.0));

    return (context.state.tempString);
}



void LoadIniFile(RecordingContext& context)
{
    const comskip::localization::Translator translator;
    LoadIniFile(context, translator);
}

void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator)
{
    if (!context.state.ini_file.get()) {
        FindIniFile(context);
        if (*context.state.inifilename) context.state.ini_file.reset(myfopen(context.state.inifilename, "r"));
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
            context.state.ini_file.reset();
            if (failed) throw std::runtime_error("Could not read INI file");
            comskip::config::Ini ini(data);
            context.settings = comskip::config::load_settings(ini, context.settings);
            context.state.ini_text += ini.serialize();
            fputs(translator.format("using_settings", context.state.inifilename).c_str(), stdout);
        }
        for (const char* file : {context.settings.cutscenefile1.c_str(), context.settings.cutscenefile2.c_str(), context.settings.cutscenefile3.c_str(), context.settings.cutscenefile4.c_str(),
                                 context.settings.cutscenefile5.c_str(), context.settings.cutscenefile6.c_str(), context.settings.cutscenefile7.c_str(), context.settings.cutscenefile8.c_str()})
            if (*file) LoadCutScene(context, file);
    } catch (const std::exception& error) {
        fputs(translator.format("invalid_configuration", error.what()).c_str(), stderr);
        comskip::request_exit(1);
    }
    if (context.settings.added_recording > 0 && context.settings.giveUpOnLogoSearch < context.settings.added_recording * 60)
        context.settings.giveUpOnLogoSearch += context.settings.added_recording * 60;
}

void list_codecs(const comskip::localization::Translator& translator);

FILE* LoadSettings(RecordingContext& context, int argc, char ** argv, const comskip::localization::Translator& translator)
{
    comskip::platform::FilePtr logo_file;
    comskip::platform::FilePtr log_file;
    comskip::platform::FilePtr test_file;
    int					i = 0;
//	int					play_nice_start = -1;
//	int					play_nice_end = -1;
    time_t				ltime;
    struct tm*			now = NULL;
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
            printf("\t\"%s\"\n", argv[i]);
        }
        else
        {
            printf("\t%s\n", argv[i]);
        }
    }
    printf("\n\n");

    context.state.argument = comskip::snapshot_arguments(argc, argv);

    if (argc <= 1)
    {

#ifdef COMSKIPGUI
//			output_debugwindow = true;
#endif

        if (strstr(argv[0],"GUI"))
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
        Debug(context, 0, "%s: insufficient memory\n", context.state.progname);
        goto exit;
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
        fputs(translator.format("method_black", BLACK_FRAME).c_str(), stdout);
        fputs(translator.format("method_logo", LOGO).c_str(), stdout);
        fputs(translator.format("method_scene", SCENE_CHANGE).c_str(), stdout);
        fputs(translator.format("method_resolution", RESOLUTION_CHANGE).c_str(), stdout);
        fputs(translator.format("method_captions", CC).c_str(), stdout);
        fputs(translator.format("method_aspect", AR).c_str(), stdout);
        fputs(translator.format("method_silence", SILENCE).c_str(), stdout);
        fputs(translator.format("method_cutscenes", CUTSCENE).c_str(), stdout);
        fputs(translator.text("all_methods"), stdout);
        comskip::request_exit(2);
    }

    if (nerrors)
    {
        fputs(translator.text("usage"), stdout);
        arg_print_syntaxv(stdout, argtable, "\n\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        fputs(translator.text("available_methods"), stdout);
        fputs(translator.format("method_black", BLACK_FRAME).c_str(), stdout);
        fputs(translator.format("method_logo", LOGO).c_str(), stdout);
        fputs(translator.format("method_scene", SCENE_CHANGE).c_str(), stdout);
        fputs(translator.format("method_resolution", RESOLUTION_CHANGE).c_str(), stdout);
        fputs(translator.format("method_captions", CC).c_str(), stdout);
        fputs(translator.format("method_aspect", AR).c_str(), stdout);
        fputs(translator.format("method_silence", SILENCE).c_str(), stdout);
        fputs(translator.format("method_cutscenes", CUTSCENE).c_str(), stdout);
        fputs(translator.text("all_methods"), stdout);
        fputs(translator.text("errors"), stdout);
        arg_print_errors(stdout, end, "ComSkip");
        comskip::request_exit(2);
    }

    if (strcmp(in->extension[0], ".csv") != 0 && strcmp(in->extension[0], ".txt") != 0)
    {
        comskip::checked_format(context.state.mpegfilename, "%s", in->filename[0]);



        comskip::checked_format(context.state.inbasename, "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        i = strlen(context.state.inbasename);
        while (i>0 && context.state.inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(context.state.shortbasename, &context.state.inbasename[i]);

 //       comskip::checked_format(mpegfilename, "%.*s.txt", (int)strlen(inbasename), inbasename);

        comskip::checked_format(context.state.inifilename, "%.*scomskip.ini", i, context.state.inbasename);
    }
    else if (strcmp(in->extension[0], ".csv") == 0)
    {
        context.state.loadingCSV = true;
        context.state.in_file.reset(myfopen(in->filename[0], "r"));
        fputs(translator.format("array_open", in->filename[0]).c_str(), stdout);
        if (!context.state.in_file.get())
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            comskip::request_exit(4);
        }

        comskip::checked_format(context.state.inbasename,     "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        comskip::checked_format(context.state.mpegfilename, "%.*s.mpg", (int)strlen(context.state.inbasename), context.state.inbasename);
        test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.ts", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.tp", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.dvr-ms", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.wtv", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.mp4", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.mkv", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            context.state.mpegfilename[0] = 0;
        }
        else
        {
            test_file.reset();
        }


        i = strlen(context.state.inbasename);
        while (i>0 && context.state.inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(context.state.shortbasename, &context.state.inbasename[i]);
        comskip::checked_format(context.state.inifilename, "%.*scomskip.ini", i, context.state.inbasename);
        if (context.state.mpegfilename[0] == 0) comskip::checked_format(context.state.mpegfilename, "%s.mpg", context.state.inbasename);
    }
    else if (strcmp(in->extension[0], ".txt") == 0)
    {
        context.state.loadingTXT = true;
        context.settings.output_default = false;
        context.state.in_file.reset(myfopen(in->filename[0], "r"));
        fputs(translator.format("review_open", in->filename[0]).c_str(), stdout);
        if (!context.state.in_file.get())
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            comskip::request_exit(4);
        }
        context.state.in_file.reset();
        context.state.in_file.reset();

        comskip::checked_format(context.state.inbasename,     "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        comskip::checked_format(context.state.mpegfilename, "%.*s.mpg", (int)strlen(context.state.inbasename), context.state.inbasename);
        test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.ts", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.tp", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.dvr-ms", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.wtv", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.mp4", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            comskip::checked_format(context.state.mpegfilename, "%.*s.mkv", (int)strlen(context.state.inbasename), context.state.inbasename);
            test_file.reset(myfopen(context.state.mpegfilename, "rb"));
        }
        if (!test_file)
        {
            context.state.mpegfilename[0] = 0;
        }
        else
        {
            test_file.reset();
        }

        i = strlen(context.state.inbasename);
        while (i>0 && context.state.inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(context.state.shortbasename, &context.state.inbasename[i]);
        comskip::checked_format(context.state.inifilename, "%.*scomskip.ini", i, context.state.inbasename);
//		comskip::checked_format(mpegfilename, "%s.mpg", inbasename);
    }
    else
    {
        fputs(translator.format("unsupported_input", in->extension[0]).c_str(), stdout);
        comskip::request_exit(5);
    }
    if (cl_ini->count)
    {
        comskip::checked_format(context.state.inifilename, "%s", cl_ini->filename[0]);
        fputs(translator.format("setting_ini", context.state.inifilename).c_str(), stdout);
    }
    context.state.ini_file.reset(myfopen(context.state.inifilename, "r"));

    if (cl_work_fname->count)
    {
        comskip::checked_format(context.state.shortbasename, "%s", cl_work_fname->filename[0]);
    }

    if (cl_work->count)
    {
        comskip::checked_format(context.state.outputdirname, "%s", cl_work->filename[0]);
        i = strlen(context.state.outputdirname);
        if (i > 0 && context.state.outputdirname[i-1] == PATH_SEPARATOR)
            context.state.outputdirname[i-1] = 0;
        comskip::checked_format(context.state.workbasename, "%s%c%s", context.state.outputdirname, PATH_SEPARATOR, context.state.shortbasename);
        strcpy(context.state.outbasename, context.state.workbasename);
    }
    else
    {
        context.state.outputdirname[0] = 0;
        strcpy(context.state.workbasename, context.state.inbasename);
    }


    if (out->count)
    {
        comskip::checked_format(context.state.outputdirname, "%s", out->filename[0]);
        i = strlen(context.state.outputdirname);
        if (i > 0 && context.state.outputdirname[i-1] == PATH_SEPARATOR)
            context.state.outputdirname[i-1] = 0;
        comskip::checked_format(context.state.outbasename, "%s%c%s", context.state.outputdirname, PATH_SEPARATOR, context.state.shortbasename);
    }
    else
    {
        context.state.outputdirname[0] = 0;
        strcpy(context.state.outbasename, context.state.inbasename);
    }

    if (cl_work->count && !out->count)   // --output also sets the output file location if not specified as 2nd argument.
    {
        strcpy(context.state.outbasename, context.state.workbasename);
    }


    comskip::checked_format(context.state.logofilename, "%s.logo.txt", context.state.workbasename);
    comskip::checked_format(context.state.logfilename, "%s.log", context.state.workbasename);
    comskip::checked_format(context.state.filename, "%s.txt", context.state.outbasename);
    if (strcmp(context.state.HomeDir, ".") == 0)
    {
        if (!context.state.ini_file.get())
        {
            comskip::checked_format(context.state.inifilename, "comskip.ini");
            context.state.ini_file.reset(myfopen(context.state.inifilename, "r"));
        }
        comskip::checked_format(context.state.exefilename, "comskip.exe");
        comskip::checked_format(context.state.dictfilename, "comskip.dictionary");
    }
    else
    {
        if (!context.state.ini_file.get())
        {
            comskip::checked_format(context.state.inifilename, "%s%ccomskip.ini", context.state.HomeDir, PATH_SEPARATOR);
            context.state.ini_file.reset(myfopen(context.state.inifilename, "r"));
        }
        comskip::checked_format(context.state.exefilename, "%s%ccomskip.exe", context.state.HomeDir, PATH_SEPARATOR);
        comskip::checked_format(context.state.dictfilename, "%s%ccomskip.dictionary", context.state.HomeDir, PATH_SEPARATOR);
    }

    if (cl_cut->count)
    {
        fputs(translator.format("loading_cut", cl_cut->filename[0]).c_str(), stdout);
        LoadCutScene(context, cl_cut->filename[0]);
    }

    if (cl_logo->count)
    {
        comskip::checked_format(context.state.logofilename, "%s", cl_logo->filename[0]);
        fputs(translator.format("setting_logo", context.state.logofilename).c_str(), stdout);
    }



    //	if (!loadingTXT)
    LoadIniFile(context, translator);

//	live_tv = true;

    time(&ltime);
    now = localtime(&ltime);
    mil_time = (now->tm_hour * 100) + now->tm_min;
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


#ifdef COMSKIPGUI
//		output_debugwindow = true;
#endif

    if (strstr(argv[0],"GUI") || strstr(argv[0], "-gui"))
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
        logo_file.reset(myfopen(context.state.logofilename, "r"));
        if(logo_file)
        {
            logo_file.reset();
            myremove(context.state.logofilename);
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
        logo_file.reset(myfopen(context.state.logofilename, "r"));
        if (context.state.loadingTXT)
        {
            // Do nothing to the log file
            context.settings.verbose = 0;
        }
        else if (context.state.loadingCSV)
        {
            log_file.reset(myfopen(context.state.logfilename, "w"));
            if (log_file) {
                fprintf(log_file.get(), "################################################################\n");
                fprintf(log_file.get(), "Generated using %s %s\n", COMSKIPPUBLIC, PACKAGE_STRING);
                fprintf(log_file.get(), "Loading comskip csv file - %s\n", in->filename[0]);
                fprintf(log_file.get(), "Time at start of run:\n%s", ctime(&ltime));
                fprintf(log_file.get(), "################################################################\n");
                log_file.reset();
            }
        }
        else if (logo_file)
        {
            logo_file.reset();
            log_file.reset(myfopen(context.state.logfilename, "a+"));
            if (log_file) {
                fprintf(log_file.get(), "################################################################\n");
                fprintf(log_file.get(), "Starting second pass using %s\n", context.state.logofilename);
                fprintf(log_file.get(), "Time at start of second run:\n%s", ctime(&ltime));
                fprintf(log_file.get(), "################################################################\n");
                log_file.reset();
            }
        }
        else
        {
            log_file.reset(myfopen(context.state.logfilename, "w"));
            if (log_file) {
                fprintf(log_file.get(), "################################################################\n");
                fprintf(log_file.get(), "Generated using %s %s\n", COMSKIPPUBLIC, PACKAGE_STRING);
                fprintf(log_file.get(), "Time at start of run:\n%s", ctime(&ltime));
                fprintf(log_file.get(), "################################################################\n");
                log_file.reset();
            }
        }
    }

    if (cl_playnice->count)
    {
        context.state.play_nice = true;
        Debug(context, 1, "ComSkip playing nice due as per command line.\n");
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
//		demux_pid = cl_pid->ival[0];
        sscanf(cl_pid->sval[0],"%x", &context.state.demux_pid);
        fputs(translator.format("setting_pid", std::format("{:x}", context.state.demux_pid)).c_str(), stdout);
    }



    Debug(context, 9, "Mpeg:\t%s\nExe\t%s\nLogo:\t%s\nIni:\t%s\n", context.state.mpegfilename, context.state.exefilename, context.state.logofilename, context.state.inifilename);
    Debug(context, 1, "\nDetection Methods to be used:\n");
    i = 0;
    if (context.settings.commDetectMethod & BLACK_FRAME)
    {
        i++;
        Debug(context, 1, "\t%i) Black Frame\n", i);
    }

    if (context.settings.commDetectMethod & LOGO)
    {
        i++;
        Debug(context, 1, "\t%i) Logo - Give up after %i seconds\n", i, context.settings.giveUpOnLogoSearch);
    }

    if (context.settings.commDetectMethod & CUTSCENE)
    {
//		commDetectMethod &= ~SCENE_CHANGE;
    }

    if (context.settings.commDetectMethod & SCENE_CHANGE)
    {
        i++;
        Debug(context, 1, "\t%i) Scene Change\n", i);
    }

    if (context.settings.commDetectMethod & RESOLUTION_CHANGE)
    {
        i++;
        Debug(context, 1, "\t%i) Resolution Change\n", i);
    }

    if (context.settings.commDetectMethod & CC)
    {
        i++;
        context.state.processCC = true;
        Debug(context, 1, "\t%i) Closed Captions\n", i);
    }

    if (context.settings.commDetectMethod & AR)
    {
        i++;
        Debug(context, 1, "\t%i) Aspect Ratio\n", i);
    }

    if (context.settings.commDetectMethod & SILENCE)
    {
        i++;
        Debug(context, 1, "\t%i) Silence\n", i);
    }

    if (context.settings.commDetectMethod & CUTSCENE)
    {
        i++;
        Debug(context, 1, "\t%i) CutScenes\n", i);
    }


    Debug(context, 1, "\n");
    if (context.settings.play_nice_start || context.settings.play_nice_end)
    {
        Debug(context,
            1,
            "\nComSkip throttles back from %.4i to %.4i.\nThe time is now %.4i ",
            context.settings.play_nice_start,
            context.settings.play_nice_end,
            mil_time
        );
        if (context.state.play_nice)
        {
            Debug(context, 1, "so comskip is running slowly.\n");
        }
        else
        {
            Debug(context, 1, "so it's full speed ahead!\n");
        }
    }

    Debug(context, 10, "\nSettings\n--------\n");
    Debug(context, 10, "%s\n", context.state.ini_text.c_str());
    comskip::checked_format(context.state.out_filename, "%s.txt", context.state.outbasename);


    if (!context.state.loadingTXT)
    {
        logo_file.reset(myfopen(context.state.logofilename, "r+"));
        if (logo_file)
        {
            Debug(context, 1, "The logo mask file exists.\n");
            logo_file.reset();
            LoadLogoMaskData(context);
        }
    }

    context.state.out_file.reset();
    context.state.plist_cutlist_file.reset();
    context.state.zoomplayer_cutlist_file.reset();
    context.state.zoomplayer_chapter_file.reset();
    context.state.vcf_file.reset();
    context.state.vdr_file.reset();
    context.state.scf_file.reset();
    context.state.projectx_file.reset();
    context.state.avisynth_file.reset();
    context.state.cuttermaran_file.reset();
    context.state.videoredo_file.reset();
    context.state.videoredo3_file.reset();
    context.state.btv_file.reset();
    context.state.edl_file.reset();
    context.state.ffmeta_file.reset();
    context.state.ffsplit_file.reset();
    context.state.live_file.reset();
    context.state.ipodchap_file.reset();
    context.state.edlp_file.reset();
    context.state.edlx_file.reset();
    context.state.mls_file.reset();
    context.state.womble_file.reset();
    context.state.mpgtx_file.reset();
    context.state.dvrcut_file.reset();
    context.state.dvrmstb_file.reset();
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
            context.state.out_file.reset(myfopen(context.state.out_filename, "w"));
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

    if (!context.state.loadingTXT && (context.settings.output_srt || context.settings.output_smi ))
    {
#ifdef PROCESS_CC


        i = 0;

        context.state.LoadSettings_CEW_argv[i++] = context.state.LoadSettings_caption_arg_0;
        if (context.settings.output_smi)
        {

        context.state.LoadSettings_CEW_argv[i++] = context.state.LoadSettings_caption_arg_1;
            context.settings.output_srt = 1;
            comskip::checked_format(context.state.LoadSettings_filename, "%s.smi", context.state.outbasename);
        }
        else
        {

        context.state.LoadSettings_CEW_argv[i++] = context.state.LoadSettings_caption_arg_2;
            comskip::checked_format(context.state.LoadSettings_filename, "%s.srt", context.state.outbasename);
        }
        context.state.LoadSettings_CEW_argv[i++] = (char *)in->filename[0];

        context.state.LoadSettings_CEW_argv[i++] = context.state.LoadSettings_caption_arg_3;
        context.state.LoadSettings_CEW_argv[i++] = context.state.LoadSettings_filename;
        CEW_init (i, context.state.LoadSettings_CEW_argv);
#endif
    }


    if (context.state.loadingCSV)
    {
        context.settings.output_framearray = false;
        ProcessCSV(context, std::move(context.state.in_file));
        context.settings.output_debugwindow = false;
    }


exit:
    return (context.state.in_file.get());
}

/*
#ifdef notused

int GetAvgBrightness(void) {
    int brightness = 0;
    int pixels = 0;
    int x;
    int y;
    for (y = border; y < (height - border); y += 4) {
        for (x = border; x < (width - border); x += 4) {
            brightness += frame_ptr[y * width + x];
            pixels++;
        }
    }

    return (brightness / pixels);
}

bool CheckFrameIsBlack(void) {
    int			x;
    int			y;
    int			pass;
    int			avg = 0;
    const int	pass_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
    const int	pass_inc[7] = { 8, 8, 4, 4, 2, 2, 1 };
    const int	pass_ystart[7] = { 0, 0, 4, 0, 2, 0, 1 };
    const int	pass_yinc[7] = { 8, 8, 8, 4, 4, 2, 2 };
    bool		isDim = false;
    int			dimCount = 0;
    int			pixelsChecked = 0;
    int			curMaxBright = 0;
    if (!width || !height) return (false);
    avg = GetAvgBrightness();

    // go through the image in png interlacing style testing if black
    // skip region 'border' pixels wide/high around border of image.
    for (pass = 0; pass < 7; pass++) {
        for (y = pass_ystart[pass] + border; y < (height - border); y += pass_yinc[pass]) {
            for (x = pass_start[pass] + border; x < (width - border); x += pass_inc[pass]) {
                pixelsChecked++;
                if (frame_ptr[y * width + x] > max_brightness) return (false);
                if (frame_ptr[y * width + x] > test_brightness) {
                    isDim = true;
                    dimCount++;
                }
            }
        }
    }

    if ((dimCount > (int)(.05 * pixelsChecked)) && (dimCount < (int)(.35 * pixelsChecked))) return (false);

    // frame is dim so test average
    if (isDim) {
        if (avg > max_avg_brightness) return (false);
    }

    brightHistogram[avg]++;
    InitializeBlackArray(black_count);
    black[black_count].frame = framenum_real;
    black[black_count].brightness = avg;
    black[black_count].uniform = 0;
    black[black_count].volume = curvolume;
    if (avg < min_brightness_found) min_brightness_found = avg;
    black_count++;
    Debug(5, "Frame %6i - Black frame with brightness of %i\n", framenum_real, avg);
    return (true);
}

void BuildBlackFrameCommList(void) {
    long		c_start[MAX_COMMERCIALS];
    long		c_end[MAX_COMMERCIALS];
    long		ic_start[MAX_COMMERCIALS];
    long		ic_end[MAX_COMMERCIALS];
    int			commercials = 0;
    int			i;
    int			j;
    int			k;
    int			x;
    int			len;
    double		remainder;
    double		added;
    bool		oldbreak;
    if (black_count == 0) return;

    // detect individual commercials from black frames
    for (i = 0; i < black_count; i++) {
        for (x = i + 1; x < black_count; x++) {
            int gap_length = black[x].frame - black[i].frame;
            if (gap_length < min_commercial_size * fps) continue;
            oldbreak = commercials > 0 && ((black[i].frame - c_end[commercials - 1]) < 10 * fps);
            if (gap_length > max_commercialbreak * fps ||
                (!oldbreak && gap_length > max_commercial_size * fps) ||
                (oldbreak && (black[x].frame - c_end[commercials - 1] > max_commercial_size * fps)))
                break;

            // if((!require_div5) || ((int)((float)gap_length/fps + .5) %5 ==
            // 0)) // look for segments in multiples of 5 seconds
            added = gap_length / fps + div5_tolerance;
            remainder = added - 5 * ((int)(added / 5.0));
            Debug(4, "%i,%i,%i: %.2f,%.2f\n", black[i].frame, black[x].frame, gap_length, gap_length / fps, remainder);
            if ((require_div5 != 1) || (remainder >= 0 && remainder <= 2 * div5_tolerance)) {

                // look for segments in multiples of 5 seconds
                if (oldbreak) {
                    if (black[x].frame > c_end[commercials - 1] + fps) {

                        // added = (black[x].frame -
                        // c_end[commercials-1])/fps;
                        c_end[commercials - 1] = black[x].frame;
                        ic_end[commercials - 1] = x;
                        Debug(
                            1,
                            "--start: %i, end: %i, len: %.2fs\t%.2fs\n",
                            black[i].frame,
                            black[x].frame,
                            (black[x].frame - black[i].frame) / fps,
                            (c_end[commercials - 1] - c_start[commercials - 1]) / fps
                        );
                    }
                } else {

                    // new break
                    Debug(
                        1,
                        "\n  start: %i, end: %i, len: %.2fs\n",
                        black[i].frame,
                        black[x].frame,
                        ((black[x].frame - black[i].frame) / fps)
                    );
                    ic_start[commercials] = i;
                    ic_end[commercials] = x;
                    c_start[commercials] = black[i].frame;
                    c_end[commercials++] = black[x].frame;
                    Debug(
                        1,
                        "\n  start: %i, end: %i, len: %is\n",
                        c_start[commercials - 1],
                        c_end[commercials - 1],
                        (int)((c_end[commercials - 1] - c_start[commercials - 1]) / fps)
                    );
                }

                i = x - 1;
                x = black_count;
            }
        }
    }

    Debug(1, "\n");
    if (verbose == 3 && runs == 0) {

        // list all black scene breaks
        marked = 0;
        commercials = 0;
        for (i = 0; i < black_count; i++) {
            if ((black[i].frame - marked) > 5 * fps) {
                marked = black[i].frame;
                commercials++;
                Debug(1, "%i: %i\n", commercials, marked);
            }
        }

        Debug(1, "\n\n");
    }

    if (verbose == 4 && runs == 0) {
        for (i = 0; i < black_count; i++) {
            Debug(1, "%i\n", black[i].frame);
        }

        Debug(1, "\n\n");
    }

    if (runs > 0 || require_div5 < 2) {
        Debug(1, "--------------------\n");
    }

    // print out commercial breaks skipping those that are too small or too large
    for (i = 0; i < commercials; i++) {
        len = c_end[i] - c_start[i];
        if ((len >= (int)min_commercialbreak * fps) && (len <= (int)max_commercialbreak * fps)) {

            // find the middle of the scene change, max 3 seconds.
            j = ic_start[i];
            while ((j > 0) && ((black[j].frame - black[j - 1].frame) == 1)) {

                // find beginning
                j--;
            }

            for (k = j; k < black_count; k++) {

                // find end
                if ((black[k].frame - black[j].frame) > (int)(3 * fps)) {
                    break;
                }
            }

            x = j + (int)((k - j) / 2);
            c_start[i] = black[x].frame;
            j = ic_end[i];
            while ((j < black_count) && ((black[j + 1].frame - black[j].frame) == 1)) {

                // find end
                j++;
            }

            for (k = j; k > 0; k--) {

                // find start
                if (black[j].frame - (black[k].frame) > (int)(3 * fps)) {
                    break;
                }
            }

            x = k + (int)((j - k) / 2);
            c_end[i] = black[x].frame - 1;
            Debug(4, "%i - start: %i   end: %i\n", i + 1, c_start[i], c_end[i]);
            if (require_div5 != 2) OutputCommercialBlock(c_start[i], c_end[i]);
        }
    }

    if (require_div5 == 2) {
        require_div5 = 1;
        runs++;
        BuildBlackFrameCommList();
    }
}

#endif
*/
