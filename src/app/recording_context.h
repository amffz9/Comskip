#pragma once
#include "legacy_detection.h"
#include "settings_value.h"
#include "video_state.h"
#include "review_window.h"
#include "translator.h"
#include <memory>
#include <vector>

struct RecordingState {
    unsigned char XDSbuffer[40][100]{};
    int lastXDS= 0;
    int firstXDS= 1;
    int startXDS= 1;
    int baseXDS= 0;
    const char * ratingSystem[4]= { "MPAA", "TPG", "CE", "CF" };
    int own_histogram[4][256]{};
    int scan_step{};
    int beforeblocks[100]{};
    int afterblocks[100]{};
    int length_order[2000]{};
    int length_sorted= false;
    int min_val[10]{};
    int max_val[10]{};
    int delta_val[10]{};
    int argument_count{};
    char ** argument= NULL;
    bool initialized= false;
    const char * progname= "ComSkip";
    FILE * out_file{};
    FILE * incommercial_file= NULL;
    FILE * ini_file= NULL;
    FILE * plist_cutlist_file= NULL;
    FILE * zoomplayer_cutlist_file= NULL;
    FILE * zoomplayer_chapter_file= NULL;
    FILE * scf_file= NULL;
    FILE * vcf_file= NULL;
    FILE * vdr_file= NULL;
    FILE * projectx_file= NULL;
    FILE * avisynth_file= NULL;
    FILE * videoredo_file= NULL;
    FILE * videoredo3_file= NULL;
    FILE * btv_file= NULL;
    FILE * edl_file= NULL;
    FILE * ffmeta_file= NULL;
    FILE * ffsplit_file= NULL;
    FILE * live_file= NULL;
    FILE * ipodchap_file= NULL;
    FILE * edlp_file= NULL;
    FILE * bcf_file= NULL;
    FILE * edlx_file= NULL;
    FILE * cuttermaran_file= NULL;
    FILE * chapters_file= NULL;
    FILE * log_file= NULL;
    FILE * womble_file= NULL;
    FILE * mls_file= NULL;
    FILE * mpgtx_file= NULL;
    FILE * dvrcut_file= NULL;
    FILE * dvrmstb_file= NULL;
    FILE * mpeg2schnitt_file= NULL;
    FILE * tuning_file= NULL;
    FILE * training_file= NULL;
    FILE * aspect_file= NULL;
    FILE * cutscene_file= NULL;
    FILE * mkvtoolnix_chapters_file= NULL;
    FILE * mkvtoolnix_tags_file= NULL;
    int audio_channels{};
    int vo_init_done= 0;
    FILE * in_file= NULL;
    int debug_cur_segment{};
    frame_info * frame= NULL;
    long frame_count{};
    long max_frame_count{};
    schange_info * schange= NULL;
    long schange_count= 0;
    long max_schange_count= 0;
    long black_count= 0;
    black_frame_info * black= NULL;
    long max_black_count{};
    struct block_info cblock[1000]{};
    long block_count= 0;
    long max_block_count{};
    logo_block_info * logo_block= NULL;
    long logo_block_count= 0;
    long max_logo_block_count{};
    bool processCC{};
    ccPacket lastcc{};
    ccPacket cc{};
    cc_block_info * cc_block= NULL;
    long cc_block_count= 0;
    long max_cc_block_count{};
    int last_cc_type= NONE;
    int current_cc_type= NONE;
    bool cc_on_screen= false;
    bool cc_in_memory= false;
    XDS_block_info * XDS_block= NULL;
    long XDS_block_count= 0;
    long max_XDS_block_count{};
    cc_text_info * cc_text= NULL;
    long cc_text_count= 0;
    long max_cc_text_count= 0;
    ar_block_info * ar_block= NULL;
    long ar_block_count= 0;
    long max_ar_block_count{};
    int last_audio_channels= 2;
    double last_ar_ratio= 0.0;
    double ar_ratio_trend= 0.0;
    int ar_ratio_trend_counter= 0;
    int ar_ratio_start= 0;
    int ar_misratio_trend_counter= 0;
    int ar_misratio_start= 0;
    ac_block_info * ac_block= NULL;
    long ac_block_count= 0;
    long max_ac_block_count{};
    commercial_list_info * commercial_list= NULL;
    int commercial_count= -1;
    Legacy_commercial_entry commercial[100000]{};
    int reffer_count= -1;
    Legacy_reffer_entry reffer[100000]{};
    Legacy_ar_histogram_entry ar_histogram[1000]{};
    double dominant_ar{};
    Legacy_ac_histogram_entry ac_histogram[12]{};
    int dominant_ac{};
    int use_cuvid{};
    int use_vdpau{};
    int use_dxva2{};
    int use_qsv{};
    int dvrms_live_tv_retries{};
    int standoff{};
    int dvrmsstandoff= 120000;
    char incomingCommandLine[260]{};
    char logofilename[260]{};
    char logfilename[260]{};
    char mpegfilename[260]{};
    char exefilename[260]{};
    char inbasename[260]{};
    char workbasename[260]{};
    char outbasename[260]{};
    char shortbasename[260]{};
    char inifilename[260]{};
    char dictfilename[260]{};
    char out_filename[260]{};
    char incommercial_filename[260]{};
    char outputdirname[260]{};
    char filename[260]{};
    int curvolume= -1;
    int ascr{};
    int scr{};
    int framenum_real{};
    int frames_with_logo{};
    int framesprocessed= 0;
    char HomeDir[256]{};
    char tempString[256]{};
    double average_score{};
    int brightness= 0;
    long sum_brightness=0;
    long sum_count{};
    int uniformHistogram[256]{};
    int brightHistogram[256]{};
    int blackHistogram[256]{};
    int volumeHistogram[256]{};
    int silenceHistogram[256]{};
    int logoHistogram[256]{};
    int volumeScale= 10;
    int last_brightness= 0;
    int min_brightness_found{};
    int min_volume_found{};
    int max_logo_gap{};
    int max_nonlogo_block_length{};
    double logo_overshoot{};
    double logo_quality{};
    int width{};
    int old_width{};
    int videowidth{};
    int height{};
    int old_height{};
    int ar_width= 0;
    int subsample_video= 0x1ff;
    char haslogo[38400000]{};
    int selftest{};
    double avg_fps= 22;
    int min_hasBright= 255000;
    int min_dimCount= 255000;
    int validate_ar= true;
    int min_volume=0;
    int min_uniform= 0;
    std::string ini_text{};
    bool play_nice= false;
    int after_start= 0;
    int before_end= 0;
    int doublCheckLogoCount= 0;
    bool output_console{};
    bool framearray= true;
    bool only_strict= false;
    int schange_threshold= 90;
    int schange_cutlevel= 15;
    double ar_rounding= 100;
    long avg_brightness= 0;
    long maxi_volume= 0;
    long avg_volume= 0;
    long avg_silence= 0;
    long avg_uniform= 0;
    double avg_schange= 0.0;
    double dictionary_modifier= 1.05;
    bool detectBlackFrames{};
    bool detectSceneChanges{};
    int dummy1{};
    unsigned char * frame_ptr{};
    int dummy2{};
    bool sceneHasChanged{};
    int sceneChangePercent{};
    bool lastFrameWasBlack= false;
    int lastFrameWasSceneChange{};
    long histogram[256]{};
    long lastHistogram[256]{};
    int cutscenematch{};
    int cutscenes=0;
    unsigned char cutscene[8][120000]{};
    int csbrightness[8]{};
    int cslength[8]{};
    char debugText[20000]{};
    bool logoInfoAvailable{};
    bool secondLogoSearch= false;
    bool logoBuffersFull= false;
    int logoTrendCounter= 0;
    double logoFreq= 1.0;
    bool lastLogoTest= false;
    int * logoFrameNum= NULL;
    int oldestLogoBuffer{};
    bool curLogoTest= false;
    int minHitsForTrend= 10;
    double logoPercentage= 0.0;
    bool reverseLogoLogic= false;
    unsigned char horiz_count[38400000]{};
    unsigned char vert_count[38400000]{};
    double borderIgnore= .05;
    int int_edge_radius= 2;
    int edge_count= 0;
    int hedge_count= 0;
    int vedge_count= 0;
    int newestLogoBuffer= -1;
    unsigned char ** logoFrameBuffer= NULL;
    int logoFrameBufferSize= 0;
    int lwidth{};
    int lheight{};
    int tlogoMinX{};
    int tlogoMaxX{};
    int tlogoMinY{};
    int tlogoMaxY{};
    int edgemask_filled=0;
    unsigned char thoriz_edgemask[38400000]{};
    unsigned char tvert_edgemask[38400000]{};
    int clogoMinX{};
    int clogoMaxX{};
    int clogoMinY{};
    int clogoMaxY{};
    unsigned char choriz_edgemask[38400000]{};
    unsigned char cvert_edgemask[38400000]{};
    FILE * dump_data_file= (FILE *)NULL;
    uint8_t ccData[500]{};
    int ccDataLen{};
    uint8_t prevccData[500]{};
    int prevccDataLen{};
    long cc_count[5]= { 0, 0, 0, 0, 0 };
    int most_cc_type= NONE;
    unsigned char ** cc_screen= NULL;
    unsigned char ** cc_memory= NULL;
    int minY{};
    int maxY{};
    int minX{};
    int maxX{};
    bool isSecondPass= false;
    long lastFrame= 0;
    long lastFrameCommCalculated{};
    bool loadingCSV= false;
    bool loadingTXT= false;
    int helpflag= 0;
    int timeflag= 0;
    int recalculate=0;
    const char * helptext[30]=

{



    "Help: press any key to remove",

    "Key          Action",

    "Arrows	        Reposition current location",

    "PgUp/PgDn      Reposition current location",

    "Alt+PgUp/PgDn  Reposition current location by 1/2 second",

    "n/p            Jump to next/previous cutpoint",

    "e/b            Jump to next/previous end of cblock",

    "z/u            Zoom in/out on the timeline",

    "g              Graph on/off",

    "x              XDS info on/off",

    "t              Toggle current cblock between show and commercial",

    "w              Write the new cutpoints to the output files",

    "c              Dump this frame as CutScene"

    "F2             Reduce the max_volume detection level",

    "F3             Reduce the non_uniformity detection level",

    "F4             Reduce the max_avg_brighness detection level",

    "F5             Toggle frame number / timecode display",

    "",

    "During commercial break review",

    "e              Set end of commercial to this position",

    "b              Set begin of commercial to this position",

    "i              Insert a new commercial",

    "d              Delete the commercial at current location",

    "s              Jump to Start of the recording",

    "f              Jump to Finish of the recording",

     "",

    "Divide and conquer commercial break review",

    "j              Set the before marker frame",

    "k              Set the end marker frame",

    "l              Clear the marker frames",

    0

};
    double currentGoodEdge= 0.0;
    int lineStart[4800]{};
    int lineEnd[4800]{};
    unsigned char hor_edgecount[38400000]{};
    unsigned char ver_edgecount[38400000]{};
    unsigned char max_br[38400000]{};
    unsigned char min_br[38400000]{};
    unsigned char graph[115200000]{};
    int gy=0;
    int pass= 0;
    double test_pts= 0.0;
    int av_log_level=AV_LOG_INFO;
    VideoState * is{};
    DictionaryPtr myoptions{};
    std::unique_ptr<VideoState> video_owner{};
    VideoState * global_video_state{};
    int64_t pev_best_effort_timestamp= 0;
    int video_stream_index= -1;
    int audio_stream_index= -1;
    int have_frame_rate{};
    int stream_index{};
    int64_t best_effort_timestamp{};
    int coding_type{};
    FILE * mpeg2dec_in_file{};
    FILE * sample_file{};
    FILE * timing_file= 0;
    int is_AC3{};
    int AC3_rate{};
    int AC3_mode{};
    int is_h264=0;
    int is_AAC=0;
    unsigned int AC3_sampling_rate{};
    int AC3_byterate{};
    int demux_pid=0;
    int demux_asf=0;
    int last_pid{};
    int pids[100]{};
    int pid_type[100]{};
    int pid_pcr[100]{};
    int pid_pid[100]{};
    int top_pid_count[8192]{};
    int top_pid_pid{};
    int pid{};
    int selected_video_pid=0;
    int selected_audio_pid=0;
    int selected_subtitle_pid=0;
    int selection_restart_count= 0;
    int found_pids=0;
    int64_t pts{};
    double initial_pts= 0.0;
    int64_t final_pts{};
    double pts_offset= 0.0;
    int initial_pts_set= 0;
    double initial_apts{};
    double apts_offset= 0.0;
    int initial_apts_set= 0;
    int do_audio_repair= 1;
    int muxrate{};
    int byterate=10000;
    int soft_seeking=0;
    char pict_type{};
    char tempstring[512]{};
    int sigint= 0;
    int decoder_verbose= 0;
    double selftest_target= 0.0;
    int framenum{};
    fpos_t filepos{};
    int64_t goppos{};
    int64_t infopos{};
    int64_t packpos{};
    int64_t ptspos{};
    int64_t headerpos{};
    int64_t frompos{};
    int64_t SeekPos{};
    int variable_bitrate{};
    int max_internal_repair_size= 40;
    int reviewing= 0;
    int count=0;
    int currentSecond=0;
    int cur_hour= 0;
    int cur_minute= 0;
    int cur_second= 0;
    int reorderCC= 0;
    int csRestart{};
    int csStartJump{};
    int csStepping{};
    int csJumping{};
    int csFound{};
    int seekIter= 0;
    int seekDirection= 0;
    int retries{};
    double base_apts= 0.0;
    double apts{};
    double top_apts= 0.0;
    short audio_buffer[1600000]{};
    short * audio_buffer_ptr= audio_buffer;
    int audio_samples= 0;
    int sound_frame_counter= 0;
    int max_volume_found= 0;
    int tracks_without_sound= 0;
    int frames_without_sound= 0;
    int frames_with_loud_sound= 0;
    uint8_t ac3_packet[100000]{};
    int ac3_packet_index= 0;
    int data_size{};
    int ac3_package_misalignment_count= 0;
    unsigned char MPEG2SysHdr[24]= {0x00, 0x00, 0x01, 0xBB, 00, 0x12, 0x80, 0x8E, 0xD3, 0x04, 0xE1, 0x7F, 0xB9, 0xE0, 0xE0, 0xB8, 0xC0, 0x54, 0xBD, 0xE0, 0x3A, 0xBF, 0xE0, 0x02};
    FILE * dump_audio_file{};
    FILE * dump_video_file{};
    int oheight= 0;
    int owidth= 0;
    double divider= 1;
    int oldfrm= -1;
    int zstart= 0;
    int zfactor= 1;
    int show_XDS=0;
    int show_silence=0;
    int preMarkerFrame= 0;
    int postMarkerFrame= 0;
    int shift= 0;
    char CauseString_cs[4][80]{};
    int CauseString_ii=0;
    unsigned char AddXDS_XDSbuf[1024]{};
    int AddXDS_c= 0;
    int DetectCredits_credit_length= 0;
    int DetectCredits_prev_credit_length= 0;
    int DetectCredits_prev_credit_end= 0;
    int DetectCredits_credit_count= 0;
    int sound_to_frames_old_c= 0;
    double sound_to_frames_old_audio_clock=0.0;
    int sound_to_frames_old_sample_rate= 0;
    uint32_t print_fps_frame_counter= 0;
    struct timeval print_fps_tv_beg{};
    struct timeval print_fps_tv_start{};
    int print_fps_total_elapsed{};
    int print_fps_last_count= 0;
    int video_packet_process_find_29fps= 0;
    int video_packet_process_force_29fps= 0;
    int video_packet_process_find_25fps= 0;
    int video_packet_process_force_25fps= 0;
    int video_packet_process_find_24fps= 0;
    int video_packet_process_force_24fps= 0;
    double video_packet_process_prev_pts= 0.0;
    double video_packet_process_prev_real_pts= 0.0;
    double video_packet_process_prev_strange_step= 0.0;
    int video_packet_process_prev_strange_framenum= 0;
    int log_callback_report_print_prefix= 1;
    char LoadSettings_filename[260]{};
    char * LoadSettings_CEW_argv[10]{};
    char LoadSettings_caption_arg_0[12]= "comskip.exe";
    char LoadSettings_caption_arg_1[6]= "-sami";
    char LoadSettings_caption_arg_2[5]= "-srt";
    char LoadSettings_caption_arg_3[3]= "-o";
    std::unique_ptr<ScanWorkers> scan_workers;
    char osname[1024]{};
};

// Own one context per analysis. Functions borrow it explicitly; there is no
// process-wide current-context pointer or thread-local replacement.
struct RecordingContext {
    comskip::config::Settings settings = comskip::config::default_settings();
    RecordingState state;
    comskip::ui::ReviewWindow window;
    comskip::localization::Translator translator;
};
