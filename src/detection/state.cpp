#include "legacy_detection.h"

int				argument_count;
char **			 argument = NULL;
bool			initialized = false;
const char*		progname = "ComSkip";
FILE*			out_file = NULL;
FILE*			incommercial_file = NULL;
FILE*			ini_file = NULL;
FILE*			plist_cutlist_file = NULL;
FILE*			zoomplayer_cutlist_file = NULL;
FILE*			zoomplayer_chapter_file = NULL;
FILE*			scf_file = NULL;
FILE*			vcf_file = NULL;
FILE*			vdr_file = NULL;
FILE*			projectx_file = NULL;
FILE*			avisynth_file = NULL;
FILE*			videoredo_file = NULL;
FILE*			videoredo3_file = NULL;
FILE*			btv_file = NULL;
FILE*			edl_file = NULL;
FILE*			ffmeta_file = NULL;
FILE*			ffsplit_file = NULL;
FILE*			live_file = NULL;
FILE*			ipodchap_file = NULL;
FILE*			edlp_file = NULL;
FILE*			bcf_file = NULL;
FILE*			edlx_file = NULL;
FILE*			cuttermaran_file = NULL;
FILE*			chapters_file = NULL;
FILE*			log_file = NULL;
FILE*			womble_file = NULL;
FILE*			mls_file = NULL;
FILE*			mpgtx_file = NULL;
FILE*			dvrcut_file = NULL;
FILE*			dvrmstb_file = NULL;
FILE*			mpeg2schnitt_file = NULL;
FILE*			tuning_file = NULL;
FILE*			training_file = NULL;
FILE*			aspect_file = NULL;
FILE*			cutscene_file = NULL;
FILE*			mkvtoolnix_chapters_file = NULL;
FILE*			mkvtoolnix_tags_file = NULL;
int audio_channels;
int				vo_init_done = 0;
FILE*	in_file = NULL;
int debug_cur_segment;
frame_info*			frame = NULL;
long				frame_count = 0;
long				max_frame_count;
schange_info*			schange = NULL;
long					schange_count = 0;
long					max_schange_count = 0;
long						black_count = 0;
black_frame_info*			black = NULL;
long						max_black_count;
struct block_info cblock[MAX_BLOCKS];
long				block_count = 0;
long				max_block_count;
logo_block_info*			logo_block = NULL;
long						logo_block_count = 0;
long						max_logo_block_count;
bool					processCC = false;
ccPacket		lastcc;
ccPacket		cc;
cc_block_info*			cc_block = NULL;
long					cc_block_count = 0;
long					max_cc_block_count;
int						last_cc_type = NONE;
int						current_cc_type = NONE;
bool					cc_on_screen = false;
bool					cc_in_memory = false;
XDS_block_info*			XDS_block = NULL;
long					XDS_block_count = 0;
long					max_XDS_block_count;
cc_text_info*			cc_text = NULL;
long					cc_text_count = 0;
long					max_cc_text_count = 0;
ar_block_info*			ar_block = NULL;
long					ar_block_count = 0;
long					max_ar_block_count;
int 	last_audio_channels = 2;
double	last_ar_ratio = 0.0;
double  ar_ratio_trend = 0.0;
int		ar_ratio_trend_counter = 0;
int		ar_ratio_start	= 0;
int		ar_misratio_trend_counter = 0;
int		ar_misratio_start	= 0;
ac_block_info*			ac_block = NULL;
long					ac_block_count = 0;
long					max_ac_block_count;
commercial_list_info*	commercial_list = NULL;
int		commercial_count = -1;
Legacy_commercial_entry commercial[MAX_COMMERCIALS];
int		reffer_count = -1;
Legacy_reffer_entry reffer[MAX_COMMERCIALS];
Legacy_ar_histogram_entry ar_histogram[MAX_ASPECT_RATIOS];
double	dominant_ar;
Legacy_ac_histogram_entry ac_histogram[MAX_AUDIO_CHANNELS];
int	dominant_ac;
int                     use_cuvid = 0;
int                     use_vdpau = 0;
int                     use_dxva2 = 0;
int                     use_qsv = 0;
int						dvrms_live_tv_retries = 300;
int						standoff = 0;
int						dvrmsstandoff = 120000;
char					incomingCommandLine[MAX_ARG];
char					logofilename[MAX_PATH];
char					logfilename[MAX_PATH];
char					mpegfilename[MAX_PATH];
char					exefilename[MAX_PATH];
char					inbasename[MAX_PATH];
char					workbasename[MAX_PATH];
char					outbasename[MAX_PATH];
char					shortbasename[MAX_PATH];
char					inifilename[MAX_PATH];
char					dictfilename[MAX_PATH];
char					out_filename[MAX_PATH];
char					incommercial_filename[MAX_PATH];
char					outputdirname[MAX_PATH];
char					filename[MAX_PATH];
int						curvolume = -1;
int			ascr,scr;
int						framenum_real;
int						frames_with_logo;
int						framesprocessed = 0;
char					HomeDir[256];
char					tempString[256];
double					average_score;
int						brightness = 0;
long						sum_brightness=0;
long					sum_count;
int						uniformHistogram[256];
int						brightHistogram[256];
int						blackHistogram[256];
int						volumeHistogram[256];
int						silenceHistogram[256];
int						logoHistogram[256];
int						volumeScale = 10;
int						last_brightness = 0;
int						min_brightness_found;
int						min_volume_found;
int						max_logo_gap;
int						max_nonlogo_block_length;
double					logo_overshoot;
double					logo_quality;
int						width, old_width, videowidth;
int						height, old_height;
int						ar_width = 0;
int						subsample_video = 0x1ff;
char haslogo[MAXWIDTH*MAXHEIGHT];
int					selftest = 0;
double              avg_fps = 22;
int					min_hasBright = 255000;
int					min_dimCount = 255000;
int					validate_ar = true;
int					min_volume=0;
int					min_uniform = 0;
std::string ini_text;
bool				play_nice = false;
int					after_start = 0;
int					before_end = 0;
int					doublCheckLogoCount = 0;
bool				output_console = true;
bool				framearray = true;
bool				only_strict = false;
int					schange_threshold = 90;
int					schange_cutlevel = 15;
double				ar_rounding = 100;
long				avg_brightness = 0;
long				maxi_volume = 0;
long				avg_volume = 0;
long				avg_silence = 0;
long				avg_uniform = 0;
double				avg_schange = 0.0;
double				dictionary_modifier = 1.05;
bool				detectBlackFrames;
bool				detectSceneChanges;
int             dummy1;
unsigned char*		frame_ptr = 0;
int dummy2;
bool				sceneHasChanged;
int					sceneChangePercent;
bool				lastFrameWasBlack = false;
bool				lastFrameWasSceneChange = false;
long histogram[256];
long lastHistogram[256];
int					cutscenematch;
int					cutscenes=0;
unsigned char		cutscene[MAXCUTSCENES][MAXCSLENGTH];
int					csbrightness[MAXCUTSCENES];
int					cslength[MAXCUTSCENES];
char				debugText[20000];
bool				logoInfoAvailable;
bool				secondLogoSearch = false;
bool				logoBuffersFull = false;
int					logoTrendCounter = 0;
double				logoFreq = 1.0;
bool				lastLogoTest = false;
int*				logoFrameNum = NULL;
int					oldestLogoBuffer;
bool				curLogoTest = false;
int					minHitsForTrend = 10;
double				logoPercentage = 0.0;
bool				reverseLogoLogic = false;
unsigned char horiz_count[MAXHEIGHT*MAXWIDTH];
unsigned char vert_count[MAXHEIGHT*MAXWIDTH];
double				borderIgnore = .05;
int					int_edge_radius = 2;
int					edge_count = 0;
int					hedge_count = 0;
int					vedge_count = 0;
int					newestLogoBuffer = -1;
unsigned char **	logoFrameBuffer = NULL;
int					logoFrameBufferSize = 0;
int					lwidth;
int					lheight;
int					tlogoMinX;
int					tlogoMaxX;
int					tlogoMinY;
int					tlogoMaxY;
int                 edgemask_filled=0;
unsigned char thoriz_edgemask[MAXHEIGHT*MAXWIDTH];
unsigned char tvert_edgemask[MAXHEIGHT*MAXWIDTH];
int					clogoMinX;
int					clogoMaxX;
int					clogoMinY;
int					clogoMaxY;
unsigned char choriz_edgemask[MAXHEIGHT*MAXWIDTH];
unsigned char cvert_edgemask[MAXHEIGHT*MAXWIDTH];
FILE *dump_data_file = (FILE *)NULL;
uint8_t				ccData[500];
int					ccDataLen;
uint8_t	    prevccData[500];
int			prevccDataLen;
long				cc_count[5] = { 0, 0, 0, 0, 0 };
int					most_cc_type = NONE;
unsigned char **	cc_screen = NULL;
unsigned char **	cc_memory = NULL;
int					minY;
int					maxY;
int					minX;
int					maxX;
bool				isSecondPass = false;
long				lastFrame = 0;
long				lastFrameCommCalculated = 0;
bool				loadingCSV = false;
bool				loadingTXT = false;
int					helpflag = 0;
int				    timeflag = 0;
int					recalculate=0;
const char *helptext[]=
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
double	currentGoodEdge = 0.0;
int lineStart[MAXHEIGHT];
int lineEnd[MAXHEIGHT];
unsigned char hor_edgecount[MAXHEIGHT*MAXWIDTH];
unsigned char ver_edgecount[MAXHEIGHT*MAXWIDTH];
unsigned char max_br[MAXHEIGHT*MAXWIDTH];
unsigned char min_br[MAXHEIGHT*MAXWIDTH];
unsigned char graph[MAXHEIGHT*MAXWIDTH*3];
int gy=0;
double get_frame_pts(int f) {
    if (!frame) {
            return(f / fps);
    }
    if (f < 1)
        f = 1;
    if (f > frame_count -1)
        f = frame_count -1;
    return(frame[f].pts);
}