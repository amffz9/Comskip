#include "legacy_detection.h"
#include "checked_format.h"
#include "translator.h"

double FindNumber(char* data, const char* key, double fallback)
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
        Debug(0, "Invalid logo metadata: %s\n", error.what());
        return fallback;
    }
}

char* intSecondsToStrMinutes(int seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(tempString, "%i:%.2i:%.2i", hours, minutes, seconds);
    return (tempString);
}

char* dblSecondsToStrMinutes(double seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(tempString, "%0i:%.2i:%.2d.%.2d", hours, minutes, (int)seconds, (int)((seconds - (int)(seconds))*100) );

    return (tempString);
}

char* dblSecondsToStrMinutesFrames(double seconds)
{
    int minutes, hours;
    hours = (int)(seconds / 3600);
    seconds -= hours * 60 * 60;
    minutes = (int)(seconds / 60);
    seconds -= minutes * 60;
    comskip::checked_format(tempString, "%0i:%.2i:%.2d.%.2d", hours, minutes, (int)seconds, (int)(((int)((seconds - (int)(seconds))*100.0)) * fps / 100.0));

    return (tempString);
}



void LoadIniFile()
{
    const comskip::localization::Translator translator;
    LoadIniFile(translator);
}

void LoadIniFile(const comskip::localization::Translator& translator)
{
    if (!ini_file) {
        FindIniFile();
        if (*inifilename) ini_file = myfopen(inifilename, "r");
    }
    try {
        ini_text = comskip::config::defaults().serialize();
        if (ini_file) {
            std::string data;
            char buffer[4096];
            std::size_t count;
            while ((count = fread(buffer, 1, sizeof buffer, ini_file)) != 0) data.append(buffer, count);
            bool failed = ferror(ini_file) != 0;
            fclose(ini_file);
            ini_file = nullptr;
            if (failed) throw std::runtime_error("Could not read INI file");
            comskip::config::Ini ini(data);
            comskip::config::apply_settings(ini);
            ini_text += ini.serialize();
            fputs(translator.format("using_settings", inifilename).c_str(), stdout);
        }
        for (const char* file : {cutscenefile1, cutscenefile2, cutscenefile3, cutscenefile4,
                                 cutscenefile5, cutscenefile6, cutscenefile7, cutscenefile8})
            if (*file) LoadCutScene(file);
    } catch (const std::exception& error) {
        fputs(translator.format("invalid_configuration", error.what()).c_str(), stderr);
        exit(1);
    }
    if (added_recording > 0 && giveUpOnLogoSearch < added_recording * 60)
        giveUpOnLogoSearch += added_recording * 60;
}

void list_codecs();

FILE* LoadSettings(int argc, char ** argv, const comskip::localization::Translator& translator)
{
//	FILE*				ini_file = NULL;
    FILE*				logo_file = NULL;
    FILE*				log_file = NULL;
    FILE*				test_file = NULL;
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

    argument = static_cast<char **>( malloc(sizeof(char *) * argc) );
    argument_count = argc;
    for (i = 0; i < argc; i++)
    {
        argument[i] = static_cast<char *>( malloc(sizeof(char) * (strlen(argv[i]) + 1)) );
        strcpy(argument[i], argv[i]);
    }

    if (argc <= 1)
    {

#ifdef COMSKIPGUI
//			output_debugwindow = true;
#endif

        if (strstr(argv[0],"GUI"))
            output_debugwindow = true;
        if (output_debugwindow)
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
        Debug(0, "%s: insufficient memory\n", progname);
        goto exit;
    }

    nerrors = arg_parse(argc, argv, argtable);
    if (cl_list_decoders->count)
    {
        list_codecs();
        exit(2);
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
        exit(2);
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
        exit(2);
    }

    if (strcmp(in->extension[0], ".csv") != 0 && strcmp(in->extension[0], ".txt") != 0)
    {
        comskip::checked_format(mpegfilename, "%s", in->filename[0]);
        /*		in_file = myfopen(in->filename[0], "rb");
                fputs(translator.format("opening", in->filename[0]).c_str(), stdout);
        		if (!in_file) {
                    fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
        			exit(3);
        		}
        */

/*
        i = mystat(( char *)in->filename[0], &instat);
        if (i <0)
               {
                   fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
                   exit(3);

               }
*/
        comskip::checked_format(inbasename, "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        i = strlen(inbasename);
        while (i>0 && inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(shortbasename, &inbasename[i]);

 //       comskip::checked_format(mpegfilename, "%.*s.txt", (int)strlen(inbasename), inbasename);
/*
        test_file = mymyfopen(mpegfilename, "w");
        if (!test_file)
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            exit(3);
        }
*/
        comskip::checked_format(inifilename, "%.*scomskip.ini", i, inbasename);
    }
    else if (strcmp(in->extension[0], ".csv") == 0)
    {
        loadingCSV = true;
        in_file = myfopen(in->filename[0], "r");
        fputs(translator.format("array_open", in->filename[0]).c_str(), stdout);
        if (!in_file)
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            exit(4);
        }

        comskip::checked_format(inbasename,     "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        comskip::checked_format(mpegfilename, "%.*s.mpg", (int)strlen(inbasename), inbasename);
        test_file = myfopen(mpegfilename, "rb");
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.ts", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.tp", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.dvr-ms", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.wtv", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.mp4", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.mkv", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            mpegfilename[0] = 0;
        }
        else
        {
            fclose(test_file);
        }


        i = strlen(inbasename);
        while (i>0 && inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(shortbasename, &inbasename[i]);
        comskip::checked_format(inifilename, "%.*scomskip.ini", i, inbasename);
        if (mpegfilename[0] == 0) comskip::checked_format(mpegfilename, "%s.mpg", inbasename);
    }
    else if (strcmp(in->extension[0], ".txt") == 0)
    {
        loadingTXT = true;
        output_default = false;
        in_file = myfopen(in->filename[0], "r");
        fputs(translator.format("review_open", in->filename[0]).c_str(), stdout);
        if (!in_file)
        {
            fputs(translator.format("open_failed", strerror(errno), in->filename[0]).c_str(), stderr);
            exit(4);
        }
        fclose(in_file);
        in_file = 0;

        comskip::checked_format(inbasename,     "%.*s", (int)strlen(in->filename[0]) - (int)strlen(in->extension[0]), in->filename[0]);
        comskip::checked_format(mpegfilename, "%.*s.mpg", (int)strlen(inbasename), inbasename);
        test_file = myfopen(mpegfilename, "rb");
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.ts", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.tp", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.dvr-ms", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.wtv", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.mp4", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            comskip::checked_format(mpegfilename, "%.*s.mkv", (int)strlen(inbasename), inbasename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            mpegfilename[0] = 0;
        }
        else
        {
            fclose(test_file);
        }

        i = strlen(inbasename);
        while (i>0 && inbasename[i-1] != PATH_SEPARATOR)
        {
            i--;
        }
        strcpy(shortbasename, &inbasename[i]);
        comskip::checked_format(inifilename, "%.*scomskip.ini", i, inbasename);
//		comskip::checked_format(mpegfilename, "%s.mpg", inbasename);
    }
    else
    {
        fputs(translator.format("unsupported_input", in->extension[0]).c_str(), stdout);
        exit(5);
    }
    if (cl_ini->count)
    {
        comskip::checked_format(inifilename, "%s", cl_ini->filename[0]);
        fputs(translator.format("setting_ini", inifilename).c_str(), stdout);
    }
    ini_file = myfopen(inifilename, "r");

    if (cl_work_fname->count)
    {
        comskip::checked_format(shortbasename, "%s", cl_work_fname->filename[0]);
    }

    if (cl_work->count)
    {
        comskip::checked_format(outputdirname, "%s", cl_work->filename[0]);
        i = strlen(outputdirname);
        if (i > 0 && outputdirname[i-1] == PATH_SEPARATOR)
            outputdirname[i-1] = 0;
        comskip::checked_format(workbasename, "%s%c%s", outputdirname, PATH_SEPARATOR, shortbasename);
        strcpy(outbasename, workbasename);
    }
    else
    {
        outputdirname[0] = 0;
        strcpy(workbasename, inbasename);
    }


    if (out->count)
    {
        comskip::checked_format(outputdirname, "%s", out->filename[0]);
        i = strlen(outputdirname);
        if (i > 0 && outputdirname[i-1] == PATH_SEPARATOR)
            outputdirname[i-1] = 0;
        comskip::checked_format(outbasename, "%s%c%s", outputdirname, PATH_SEPARATOR, shortbasename);
    }
    else
    {
        outputdirname[0] = 0;
        strcpy(outbasename, inbasename);
    }

    if (cl_work->count && !out->count)   // --output also sets the output file location if not specified as 2nd argument.
    {
        strcpy(outbasename, workbasename);
    }


    comskip::checked_format(logofilename, "%s.logo.txt", workbasename);
    comskip::checked_format(logfilename, "%s.log", workbasename);
    comskip::checked_format(filename, "%s.txt", outbasename);
    if (strcmp(HomeDir, ".") == 0)
    {
        if (!ini_file)
        {
            comskip::checked_format(inifilename, "comskip.ini");
            ini_file = myfopen(inifilename, "r");
        }
        comskip::checked_format(exefilename, "comskip.exe");
        comskip::checked_format(dictfilename, "comskip.dictionary");
    }
    else
    {
        if (!ini_file)
        {
            comskip::checked_format(inifilename, "%s%ccomskip.ini", HomeDir, PATH_SEPARATOR);
            ini_file = myfopen(inifilename, "r");
        }
        comskip::checked_format(exefilename, "%s%ccomskip.exe", HomeDir, PATH_SEPARATOR);
        comskip::checked_format(dictfilename, "%s%ccomskip.dictionary", HomeDir, PATH_SEPARATOR);
    }

    if (cl_cut->count)
    {
        fputs(translator.format("loading_cut", cl_cut->filename[0]).c_str(), stdout);
        LoadCutScene(cl_cut->filename[0]);
    }

    if (cl_logo->count)
    {
        comskip::checked_format(logofilename, "%s", cl_logo->filename[0]);
        fputs(translator.format("setting_logo", logofilename).c_str(), stdout);
    }



    //	if (!loadingTXT)
    LoadIniFile(translator);

//	live_tv = true;

    time(&ltime);
    now = localtime(&ltime);
    mil_time = (now->tm_hour * 100) + now->tm_min;
    if ((play_nice_start > -1) && (play_nice_end > -1))
    {
        if (play_nice_start > play_nice_end)
        {
            if ((mil_time >= play_nice_start) || (mil_time <= play_nice_end)) play_nice = true;
        }
        else
        {
            if ((mil_time >= play_nice_start) && (mil_time <= play_nice_end)) play_nice = true;
        }
    }

    if (cl_verbose->count)
    {
        verbose = cl_verbose->ival[0];
        fputs(translator.format("setting_verbose", verbose).c_str(), stdout);
    }

    if (cl_selftest->count)
    {
        selftest = cl_selftest->ival[0];
        fputs(translator.format("setting_selftest", selftest).c_str(), stdout);
    }

    if (cl_debugwindow->count || loadingTXT)
    {
        output_debugwindow = true;
    }
    if (cl_timing->count)
    {
        output_timing = true;
    }
    if (cl_show->count)
    {
        subsample_video = 0;
        output_debugwindow = true;
    }

    if (cl_quiet->count)
    {
        output_console = false;
    }


#ifdef COMSKIPGUI
//		output_debugwindow = true;
#endif

    if (strstr(argv[0],"GUI") || strstr(argv[0], "-gui"))
        output_debugwindow = true;

    if (cl_demux->count)
    {
        output_demux = true;
    }

    if (cl_hwassist->count)
    {
        hardware_decode = 1;
    }
    if (cl_use_cuvid->count)
    {
        fputs(translator.text("enable_cuvid"), stdout);
        use_cuvid = 1;
    }
    if (cl_use_vdpau->count)
    {
        fputs(translator.text("enable_vdpau"), stdout);
        use_vdpau = 1;
    }

    if (cl_use_dxva2->count)
    {
        fputs(translator.text("enable_dxva2"), stdout);
        use_dxva2 = 1;
    }

    if (cl_use_qsv->count)
    {
        fputs(translator.text("enable_qsv"), stdout);
        use_qsv = 1;
    }

    if (cl_threads->count)
    {
        thread_count = cl_threads->ival[0];
    }

    if (!loadingTXT && !useExistingLogoFile && cl_logo->count==0)
    {
        logo_file = myfopen(logofilename, "r");
        if(logo_file)
        {
            fclose(logo_file);
            myremove(logofilename);
        }
    }

    if (cl_output_csv->count)
    {
        output_framearray = true;
    }

    if (cl_output_training->count)
    {
        output_training = true;
    }



    if (verbose)
    {
        logo_file = myfopen(logofilename, "r");
        if (loadingTXT)
        {
            // Do nothing to the log file
            verbose = 0;
        }
        else if (loadingCSV)
        {
            log_file = myfopen(logfilename, "w");
            if (log_file) {
                fprintf(log_file, "################################################################\n");
                fprintf(log_file, "Generated using %s %s\n", COMSKIPPUBLIC, PACKAGE_STRING);
                fprintf(log_file, "Loading comskip csv file - %s\n", in->filename[0]);
                fprintf(log_file, "Time at start of run:\n%s", ctime(&ltime));
                fprintf(log_file, "################################################################\n");
                fclose(log_file);
            }
            log_file = NULL;
        }
        else if (logo_file)
        {
            fclose(logo_file);
            log_file = myfopen(logfilename, "a+");
            if (log_file) {
                fprintf(log_file, "################################################################\n");
                fprintf(log_file, "Starting second pass using %s\n", logofilename);
                fprintf(log_file, "Time at start of second run:\n%s", ctime(&ltime));
                fprintf(log_file, "################################################################\n");
                fclose(log_file);
            }
            log_file = NULL;
        }
        else
        {
            log_file = myfopen(logfilename, "w");
            if (log_file) {
                fprintf(log_file, "################################################################\n");
                fprintf(log_file, "Generated using %s %s\n", COMSKIPPUBLIC, PACKAGE_STRING);
                fprintf(log_file, "Time at start of run:\n%s", ctime(&ltime));
                fprintf(log_file, "################################################################\n");
                fclose(log_file);
            }
            log_file = NULL;
        }
    }

    if (cl_playnice->count)
    {
        play_nice = true;
        Debug(1, "ComSkip playing nice due as per command line.\n");
    }

    if (cl_detectmethod->count)
    {
        commDetectMethod = cl_detectmethod->ival[0];
        fputs(translator.format("setting_methods", commDetectMethod).c_str(), stdout);
    }

    if (cl_dump->count)
    {
        cutsceneno = cl_dump->ival[0];
        fputs(translator.format("setting_dump", cutsceneno).c_str(), stdout);
    }

    if (cl_ts->count)
    {
        demux_pid = 1;
        fputs(translator.text("auto_pid"), stdout);
    }

    if (cl_pid->count)
    {
//		demux_pid = cl_pid->ival[0];
        sscanf(cl_pid->sval[0],"%x", &demux_pid);
        fputs(translator.format("setting_pid", std::format("{:x}", demux_pid)).c_str(), stdout);
    }



    Debug(9, "Mpeg:\t%s\nExe\t%s\nLogo:\t%s\nIni:\t%s\n", mpegfilename, exefilename, logofilename, inifilename);
    Debug(1, "\nDetection Methods to be used:\n");
    i = 0;
    if (commDetectMethod & BLACK_FRAME)
    {
        i++;
        Debug(1, "\t%i) Black Frame\n", i);
    }

    if (commDetectMethod & LOGO)
    {
        i++;
        Debug(1, "\t%i) Logo - Give up after %i seconds\n", i, giveUpOnLogoSearch);
    }

    if (commDetectMethod & CUTSCENE)
    {
//		commDetectMethod &= ~SCENE_CHANGE;
    }

    if (commDetectMethod & SCENE_CHANGE)
    {
        i++;
        Debug(1, "\t%i) Scene Change\n", i);
    }

    if (commDetectMethod & RESOLUTION_CHANGE)
    {
        i++;
        Debug(1, "\t%i) Resolution Change\n", i);
    }

    if (commDetectMethod & CC)
    {
        i++;
        processCC = true;
        Debug(1, "\t%i) Closed Captions\n", i);
    }

    if (commDetectMethod & AR)
    {
        i++;
        Debug(1, "\t%i) Aspect Ratio\n", i);
    }

    if (commDetectMethod & SILENCE)
    {
        i++;
        Debug(1, "\t%i) Silence\n", i);
    }

    if (commDetectMethod & CUTSCENE)
    {
        i++;
        Debug(1, "\t%i) CutScenes\n", i);
    }


    Debug(1, "\n");
    if (play_nice_start || play_nice_end)
    {
        Debug(
            1,
            "\nComSkip throttles back from %.4i to %.4i.\nThe time is now %.4i ",
            play_nice_start,
            play_nice_end,
            mil_time
        );
        if (play_nice)
        {
            Debug(1, "so comskip is running slowly.\n");
        }
        else
        {
            Debug(1, "so it's full speed ahead!\n");
        }
    }

    Debug(10, "\nSettings\n--------\n");
    Debug(10, "%s\n", ini_text.c_str());
    comskip::checked_format(out_filename, "%s.txt", outbasename);


    if (!loadingTXT)
    {
        logo_file = myfopen(logofilename, "r+");
        if (logo_file)
        {
            Debug(1, "The logo mask file exists.\n");
            fclose(logo_file);
            LoadLogoMaskData();
        }
    }

    out_file = plist_cutlist_file = zoomplayer_cutlist_file = zoomplayer_chapter_file = vcf_file = vdr_file = scf_file = projectx_file = avisynth_file = cuttermaran_file = videoredo_file = videoredo3_file = btv_file = edl_file = ffmeta_file = ffsplit_file = live_file = ipodchap_file = edlp_file = edlx_file = mls_file = womble_file = mpgtx_file = dvrcut_file = dvrmstb_file = tuning_file = training_file = 0L;

    if (cl_output_plist->count)
        output_plist_cutlist = true;
    if (cl_output_zp_cutlist->count)
        output_zoomplayer_cutlist = true;
    if (cl_output_zp_chapter->count)
        output_zoomplayer_chapter = true;
    if (cl_output_scf->count)
        output_scf = true;
    if (cl_output_vredo->count)
        output_videoredo = true;
    if (cl_output_vredo3->count)
    {
        output_videoredo3 = true;
        output_videoredo = false;
    }
    if (cl_output_plist->count)
        output_plist_cutlist = true;

    if (output_default && ! loadingTXT)
    {
        if(!isSecondPass)
        {
            out_file = myfopen(out_filename, "w");
            if (!out_file)
            {
                fputs(translator.format("create_failed", strerror(errno), filename).c_str(), stderr);
                exit(6);
            }
            else
            {
                output_default = true;
                fclose(out_file);
            }
        }
    }

//	max_commercialbreak *= fps;
//	min_commercialbreak *= fps;
//	max_commercial_size *= fps;
//	min_commercial_size *= fps;

    if (loadingTXT)
    {
        frame_count = InputReffer(".txt", true);
        if (frame_count < 0)
        {
            fputs(translator.text("incompatible_txt"), stdout);
            exit(2);
        }
        framearray = false;
        fputs(translator.text("close_window"), stdout);
        output_debugwindow = true;
        ReviewResult();
//		in_file = NULL;
    }

    if (!loadingTXT && (output_srt || output_smi ))
    {
#ifdef PROCESS_CC
static        char filename[MAX_PATH];
static        char *CEW_argv[10];
        i = 0;
        static char caption_arg_0[] = "comskip.exe";
        CEW_argv[i++] = caption_arg_0;
        if (output_smi)
        {
            static char caption_arg_1[] = "-sami";
        CEW_argv[i++] = caption_arg_1;
            output_srt = 1;
            comskip::checked_format(filename, "%s.smi", outbasename);
        }
        else
        {
            static char caption_arg_2[] = "-srt";
        CEW_argv[i++] = caption_arg_2;
            comskip::checked_format(filename, "%s.srt", outbasename);
        }
        CEW_argv[i++] = (char *)in->filename[0];
        static char caption_arg_3[] = "-o";
        CEW_argv[i++] = caption_arg_3;
        CEW_argv[i++] = filename;
        CEW_init (i, CEW_argv);
#endif
    }


    if (loadingCSV)
    {
        output_framearray = false;
        ProcessCSV(in_file);
        output_debugwindow = false;
    }


exit:
    // deallocate each non-null entry in argtable[]
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return (in_file);
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
