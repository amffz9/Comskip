#pragma once
#include "scan_geometry.h"
// Internal interfaces shared during the incremental detector migration.
//
// comskip.c
// Copyright (C) 2004 Scott Michael
// Based on the work of Chris Pinkham of MythTV
// comskip is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// comskip is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#include "platform.h"
#include "vo.h"
#include <argtable2.h>



extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

//#define restrict
//#include <libavcodec/ac3dec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

#ifdef HARDWARE_DECODE
#include <fftools/ffmpeg.h>
#endif

#include "comskip.h"
#include "commercial_length.h"
#include "settings.h"
#include <string>
#include <stdexcept>
#include <sstream>


// Define detection methods
#define BLACK_FRAME		1
#define LOGO			2
#define SCENE_CHANGE	4
#define RESOLUTION_CHANGE		8
#define CC				16
#define AR				32
#define SILENCE			64
#define	CUTSCENE		128

// Define logo detection directions
#define HORIZ	0
#define VERT	1
#define DIAG1	2
#define DIAG2	3

// Define CC types
#define NONE		0
#define ROLLUP		1
#define POPON		2
#define PAINTON		3
#define COMMERCIAL	4

// Define aspect ratio
#define FULLSCREEN		true
#define WIDESCREEN		false


#define AR_TREND	 0.8
#define DEEP_SILENCE	6	//max_volume / DEEP_SILENCE defines deep silence

#define OPEN_INPUT	1
#define OPEN_INI	2
#define SAVE_DMP	3
#define SAVE_INI	4

#ifdef DONATOR
#define COMSKIPPUBLIC "donator"
#else
#define COMSKIPPUBLIC "public"
#endif


#define MAX(X,Y) (X>Y?X:Y)
#define MIN(X,Y) (X<Y?X:Y)

// max number of frames that can be marked
#define MAX_IDENTIFIERS 300000
#define MAX_COMMERCIALS 100000
extern int argument_count;
extern char ** argument;
extern bool initialized;
extern const char * progname;
extern FILE * out_file;
extern FILE * incommercial_file;
extern FILE * ini_file;
extern FILE * plist_cutlist_file;
extern FILE * zoomplayer_cutlist_file;
extern FILE * zoomplayer_chapter_file;
extern FILE * scf_file;
extern FILE * vcf_file;
extern FILE * vdr_file;
extern FILE * projectx_file;
extern FILE * avisynth_file;
extern FILE * videoredo_file;
extern FILE * videoredo3_file;
extern FILE * btv_file;
extern FILE * edl_file;
extern FILE * ffmeta_file;
extern FILE * ffsplit_file;
extern FILE * live_file;
extern FILE * ipodchap_file;
extern FILE * edlp_file;
extern FILE * bcf_file;
extern FILE * edlx_file;
extern FILE * cuttermaran_file;
extern FILE * chapters_file;
extern FILE * log_file;
extern FILE * womble_file;
extern FILE * mls_file;
extern FILE * mpgtx_file;
extern FILE * dvrcut_file;
extern FILE * dvrmstb_file;
extern FILE * mpeg2schnitt_file;
extern FILE * tuning_file;
extern FILE * training_file;
extern FILE * aspect_file;
extern FILE * cutscene_file;
extern FILE * mkvtoolnix_chapters_file;
extern FILE * mkvtoolnix_tags_file;
extern int		demux_pid;
extern int		selected_audio_pid;
extern int		selected_subtitle_pid;
extern int		selected_video_pid;
extern int		demux_asf;

extern "C" int key;
extern "C" char osname[];

extern int audio_channels;

#define KDOWN	1
#define KUP		2
#define KLEFT	3
#define KRIGHT	4
#define KNEXT	5
#define KPREV	6
extern "C" int xPos,yPos,lMouseDown;

extern int framenum_infer;


extern int64_t headerpos;
extern int vo_init_done;
extern int soft_seeking;

extern FILE * in_file;

#undef FRAME_WITH_HISTOGRAM
#undef FRAME_WITH_LOGO
#undef FRAME_WITH_AR

typedef struct
{
//	long	frame;
    int		brightness;
    int		schange_percent;
    int		minY;
    int		maxY;
    int		uniform;
    int		volume;
    double	currentGoodEdge;
    double	ar_ratio;
    bool	logo_present;
    bool	commercial;
    int	isblack;
    int64_t		goppos;
    double	pts;
    char    pict_type;
    int		minX;
    int		maxX;
    int		hasBright;
    int		dimCount;
    int    cutscenematch;
    double logo_filter;
    int    xds;
    int cur_segment;
    int audio_channels;
#ifdef FRAME_WITH_HISTOGRAM
    int		histogram[256];
#endif
} frame_info;

extern int debug_cur_segment;

extern frame_info * frame;
extern long frame_count;
extern long max_frame_count;
						// frames per second (NTSC=29.970, PAL=25)

double get_frame_pts(int f);

#define F2V(X) (frame != NULL ? ((X) <= 0 ? frame[1].pts : ((X) >= framenum_real ? frame[framenum_real - 1].pts : frame[X].pts )) : (X) / fps)
#define assert(T) (aaa = ((T) ? 1 : *(int *)0))
//#define F2T(X) (F2V(X) - F2V(1))
#define F2T(X) (F2V(X))
#define F2L(X,Y) (F2V(X) - F2V(Y))

#define F2F(X) ((long) (F2T(X) * fps + 1.5 ))

typedef struct
{
    long	frame;
    int		percentage;
} schange_info;

extern schange_info * schange;
extern long schange_count;
extern long max_schange_count;

typedef struct
{
    long	frame;
    int		brightness;
    long	uniform;
    int		volume;
    int		cause;
} black_frame_info;

extern long black_count;
extern black_frame_info * black;
extern long max_black_count;

typedef struct block_info
{
    long			f_start;
    long			f_end;
    unsigned int	b_head;
    unsigned int	b_tail;
    unsigned int	bframe_count;
    unsigned int	schange_count;
    double			schange_rate;						// in changes per second
    double			length;
    double			score;
    int				combined_count;
    int				cc_type;
//	bool			ar;
    double			ar_ratio;
    int			audio_channels;
    int				cause;
    int				more;
    int				less;
    int				brightness;
    int				volume;
    int				silence;
    int				uniform;
    int				stdev;
    char			reffer;
    double			logo;
    double			correlation;
    int				strict;
    int				iscommercial;
} block_info;

#define MAX_BLOCKS	1000
extern struct block_info cblock[1000];

extern long block_count;
extern long max_block_count;

#define		C_c			(1<<1)
#define		C_l			(1<<0)
#define		C_s			(1<<2)
#define		C_a			(1<<5)
#define		C_u			(1<<3)
#define		C_b			(1<<4)
#define		C_r			((long)1<<29)

#define		C_STRICT	(1<<6)
#define		C_NONSTRICT	(1<<7)
#define		C_COMBINED	(1<<8)
#define		C_LOGO		(1<<9)
#define		C_EXCEEDS	(1<<10)
#define		C_AR		(1<<11)
#define		C_SC		(1<<12)
#define		C_H1		(1<<13)
#define		C_H2		(1<<14)
#define		C_H3		(1<<15)
#define		C_H4		((long)1<<16)
#define		C_H5		((long)1<<17)
#define		C_H6		((long)1<<22)
#define		C_v			((long)1<<18)
#define		C_BRIGHT	((long)1<<19)
#define		C_NOTBRIGHT	((long)1<<20)
#define		C_DIM		((long)1<<21)
#define		C_AB		((long)1<<23)
#define		C_AU		((long)1<<24)
#define		C_AL		((long)1<<25)
#define		C_AS		((long)1<<26)
#define		C_AC		((long)1<<26)
#define		C_F			((long)1<<27)
#define		C_t			((long)1<<28)
#define		C_H7		((long)1<<29)
#define		C_H8		((long)1<<30)


#define C_CUTMASK	(C_c | C_l | C_s | C_a | C_u | C_b | C_t | C_r)
#define CUTCAUSE(c) ( c & C_CUTMASK)


//int minLogo = 30;
//int maxLogo	= 120;

typedef struct
{
    int start;
    int end;
} logo_block_info;

extern logo_block_info * logo_block;
extern long logo_block_count;		// How many groups have already been identified. Increment after fill.
extern long max_logo_block_count;

extern bool processCC;
extern int					reorderCC;

typedef struct
{
    unsigned char	cc1[2];
    unsigned char	cc2[2];
} ccPacket;

extern ccPacket lastcc;
extern ccPacket cc;

typedef struct
{
    long	start_frame;
    long	end_frame;
    int		type;
} cc_block_info;

extern cc_block_info * cc_block;
extern long cc_block_count;
extern long max_cc_block_count;
extern int last_cc_type;
extern int current_cc_type;
extern bool cc_on_screen;
extern bool cc_in_memory;

typedef struct
{
    long	frame;
    char	name[40];
    int		v_chip;
    int		duration;
    int		position;
    int		composite1;
    int		composite2;
} XDS_block_info;

extern XDS_block_info * XDS_block;
extern long XDS_block_count;
extern long max_XDS_block_count;


typedef struct
{
    long			start_frame;
    long			end_frame;
    long			text_len;
    unsigned char	text[256];
} cc_text_info;

extern cc_text_info * cc_text;
extern long cc_text_count;
extern long max_cc_text_count;


#define AR_UNDEF	0.0
typedef struct
{
    int		start;
    int		end;
//	bool	ar;
    double	ar_ratio;
    int		volume;
    int		height, width;
    int		minX,maxX,minY,maxY;
} ar_block_info;

extern ar_block_info * ar_block;
extern long ar_block_count;				// How many groups have already been identified. Increment after fill.
extern long max_ar_block_count;
extern int last_audio_channels;
extern double last_ar_ratio;
extern double ar_ratio_trend;
extern int ar_ratio_trend_counter;
extern int ar_ratio_start;
extern int ar_misratio_trend_counter;
extern int ar_misratio_start;

#define AC_UNDEF	0
typedef struct
{
    int		start;
    int		end;
//	bool	ar;
    int 	audio_channels;
} ac_block_info;

extern ac_block_info * ac_block;
extern long ac_block_count;				// How many groups have already been identified. Increment after fill.
extern long max_ac_block_count;


typedef struct
{
    long	start;
    long	end;
} commercial_list_info;

extern commercial_list_info * commercial_list;

extern int commercial_count;
struct Legacy_commercial_entry
{
    long	start_frame;
    long	end_frame;
    int		start_block;
    int		end_block;
    double	length;
};
extern Legacy_commercial_entry commercial[MAX_COMMERCIALS];


extern int reffer_count;
struct Legacy_reffer_entry
{
    long	start_frame;
    long	end_frame;
};
extern Legacy_reffer_entry reffer[MAX_COMMERCIALS];



#define MAX_ASPECT_RATIOS	1000
struct Legacy_ar_histogram_entry
{
    long	frames;
    double	ar_ratio;
};
extern Legacy_ar_histogram_entry ar_histogram[MAX_ASPECT_RATIOS];
extern double dominant_ar;

#define MAX_AUDIO_CHANNELS	12
struct Legacy_ac_histogram_entry
{
    long	frames;
    int     audio_channels;
};
extern Legacy_ac_histogram_entry ac_histogram[MAX_AUDIO_CHANNELS];
extern int dominant_ac;



extern int use_cuvid;
extern int use_vdpau;
extern int use_dxva2;
extern int use_qsv;






extern int dvrms_live_tv_retries;
extern int standoff;
extern int dvrmsstandoff;
extern int standoff_retries;
extern int standoff_time;
extern int standoff_size;
extern int standoff_initial_size;
extern int standoff_initial_wait;

extern char incomingCommandLine[MAX_ARG];
extern char logofilename[MAX_PATH];
extern char logfilename[MAX_PATH];
extern char mpegfilename[MAX_PATH];
extern char exefilename[MAX_PATH];
extern char inbasename[MAX_PATH];
extern char workbasename[MAX_PATH];
extern char outbasename[MAX_PATH];
extern char shortbasename[MAX_PATH];
extern char inifilename[MAX_PATH];
extern char dictfilename[MAX_PATH];
extern char out_filename[MAX_PATH];
extern char incommercial_filename[MAX_PATH];

extern char outputdirname[MAX_PATH];
extern char filename[MAX_PATH];
extern int curvolume;
extern int						framenum;
//unsigned int			frame_period;
//int						audio_framenum = 0;
//extern int64_t			pts;
extern int64_t			initial_pts;
extern int				initial_pts_set;
extern char pict_type;

extern int ascr;
extern int scr;
extern int framenum_real;
extern int frames_with_logo;
extern int framesprocessed;
extern char HomeDir[256];					// comskip home directory
extern char tempString[256];
extern double average_score;
extern int brightness;
extern long sum_brightness;
extern long sum_count;
extern int uniformHistogram[256];
#define UNIFORMSCALE 100
extern int brightHistogram[256];
extern int blackHistogram[256];
extern int volumeHistogram[256];
extern int silenceHistogram[256];
extern int logoHistogram[256];
extern int volumeScale;
extern int last_brightness;
extern int min_brightness_found;
extern int min_volume_found;
extern int max_logo_gap;
extern int max_nonlogo_block_length;
extern double logo_overshoot;
extern double logo_quality;
extern int width;
extern int old_width;
extern int videowidth;
extern int height;
extern int old_height;
extern int ar_width;
extern int subsample_video;
//#define MAXWIDTH	2000
//#define MAXHEIGHT	1200

extern char haslogo[MAXWIDTH * MAXHEIGHT];

// unsigned char		oldframe[MAXWIDTH*MAXHEIGHT];

// variables defining options with defaults
extern int selftest;
						// show extra info
extern double avg_fps;

						// border around edge of video to ignore

						// border from bottom to ignore

						// border from bottom to ignore



				// frame not black if any pixels checked are greater than this (scale 0 to 255)

extern int min_hasBright;
extern int min_dimCount;
				// frame not pure black if any pixels are greater than this, will check average
			// maximum average brightness for a dim frame to be considered black (scale 0 to








extern int validate_ar;



extern int min_volume;
extern int min_uniform;

extern int ms_audio_delay;

//int					variable_bitrate = 1;

extern int is_h264;
///brightness (scale 0 to 255)
extern std::string ini_text;
///255)
	// maximum length in seconds to consider a segment a commercial break
	// minimum length in seconds to consider a segment a commercial break
	// maximum time in seconds for a single commercial
	// mimimum time in seconds for a single commercial

					// set=1 to only mark breaks divisible by 5 as a commercial.

extern bool play_nice;














			// If no logo is identified after x seconds into the show - give up.
			// If no logo is identified after x seconds into the show - give up.



extern int after_start;
extern int before_end;












extern int doublCheckLogoCount;












































extern bool output_console;









extern bool framearray;

extern bool only_strict;




















extern int schange_threshold;
extern int schange_cutlevel;





extern double ar_rounding;

extern long avg_brightness;
extern long maxi_volume;
extern long avg_volume;
extern long avg_silence;
extern long avg_uniform;
extern double avg_schange;
extern double dictionary_modifier;


extern bool detectBlackFrames;
extern bool detectSceneChanges;
extern int dummy1;
extern unsigned char * frame_ptr;
extern int dummy2;

// bool				frameIsBlack;
extern bool sceneHasChanged;
extern int sceneChangePercent;
extern bool lastFrameWasBlack;
extern bool lastFrameWasSceneChange;


extern long histogram[256];
extern long lastHistogram[256];

#define				MAXCSLENGTH		400*300
#define				MAXCUTSCENES	8


void LoadCutScene(const char *filename);
void RecordCutScene(int frame_count,int brightness);











extern int cutscenematch;



extern int cutscenes;
extern unsigned char cutscene[8][120000];
extern int csbrightness[8];
extern int cslength[8];

// int					cssum[MAXCUTSCENES];
// int					csmatch[MAXCUTSCENES];


extern char debugText[20000];
extern bool logoInfoAvailable;
extern bool secondLogoSearch;
extern bool logoBuffersFull;
extern int logoTrendCounter;
extern double logoFreq;						// times fps between logo checks
				// How many frames to compare at a time for logo detection;
extern bool lastLogoTest;
// int					logoTrendStartFrame;
extern int * logoFrameNum;				// Keep track of the frame numbers of each buffer
extern int oldestLogoBuffer;					// Which buffer is the oldest?
// int					lastRealLogoChange;
extern bool curLogoTest;
extern int minHitsForTrend;
// bool				hindsightLogoState = true;
extern double logoPercentage;
extern bool reverseLogoLogic;
#define MULTI_EDGE_BUFFER 0
#if MULTI_EDGE_BUFFER
unsigned char **	horiz_edges = NULL;				// rotating storage for detected horizontal edges
unsigned char **	vert_edges = NULL;					// rotating storage for detected vertical edges
#else
extern unsigned char horiz_count[MAXWIDTH * MAXHEIGHT];
extern unsigned char vert_count[MAXWIDTH * MAXHEIGHT];
#endif
extern double borderIgnore;					// Percentage of each side to ignore for logo detection




extern int int_edge_radius;



extern int edge_count;
extern int hedge_count;
extern int vedge_count;
extern int newestLogoBuffer;				// Which buffer is the newest? Increments prior to fill so start at -1
extern unsigned char ** logoFrameBuffer;			// rotating storage for frames
extern int logoFrameBufferSize;
extern int lwidth;
extern int lheight;

extern int tlogoMinX;
extern int tlogoMaxX;
extern int tlogoMinY;
extern int tlogoMaxY;
extern int edgemask_filled;
extern unsigned char thoriz_edgemask[MAXWIDTH * MAXHEIGHT];
extern unsigned char tvert_edgemask[MAXWIDTH * MAXHEIGHT];

extern int clogoMinX;
extern int clogoMaxX;
extern int clogoMinY;
extern int clogoMaxY;
extern unsigned char choriz_edgemask[MAXWIDTH * MAXHEIGHT];
extern unsigned char cvert_edgemask[MAXWIDTH * MAXHEIGHT];





extern FILE * dump_data_file;
extern uint8_t ccData[500];
extern int ccDataLen;
extern uint8_t prevccData[500];
extern int prevccDataLen;
extern long cc_count[5];
extern int most_cc_type;
extern unsigned char ** cc_screen;
extern unsigned char ** cc_memory;
extern int minY;								// The top of the picture for aspect ratio calculation
extern int maxY;								// The bottom of the picture for aspect ratio calculation
extern int minX;								// The top of the picture for aspect ratio calculation
extern int maxX;								// The bottom of the picture for aspect ratio calculation
//bool				currentAR;
//bool				lastAR;
//bool				showAvgAR;
extern bool isSecondPass;
extern long lastFrame;
extern long lastFrameCommCalculated;

extern bool loadingCSV;
extern bool loadingTXT;
extern int helpflag;
extern int timeflag;
#define MAXTIMEFLAG 2
extern int recalculate;
extern const char * helptext[30];

extern double currentGoodEdge;


extern int lineStart[MAXHEIGHT];		/* Area to include for black frame detection, non logo area */
extern int lineEnd[MAXHEIGHT];

extern unsigned char hor_edgecount[MAXWIDTH * MAXHEIGHT];
extern unsigned char ver_edgecount[MAXWIDTH * MAXHEIGHT];
extern unsigned char max_br[MAXWIDTH * MAXHEIGHT];
extern unsigned char min_br[MAXWIDTH * MAXHEIGHT];



extern unsigned char graph[MAXWIDTH * MAXHEIGHT * 3];

extern int gy;

// Function Prototypes
bool				BuildBlocks(bool recalc);
void				Recalc(void);
double				ValidateBlackFrames(long reason, double ratio, int remove);
int					DetectCommercials(int, double);
bool				BuildMasterCommList(void);
void				WeighBlocks(void);
bool				OutputBlocks(void);
void        OutputAspect(void);
void        OutputTraining(void);
bool ProcessLogoTest(int framenum_real, int curLogoTest, int close);
void        OutputStrict(double len, double delta, double tol);
int					InputReffer(const char *ext, int setfps);
bool				IsStandardCommercialLength(double length, double tolerance, bool strict);
bool				LengthWithinTolerance(double test_length, double expected_length, double tolerance);
double				FindNumber(char* str1, const char* str2, double v);
char*				intSecondsToStrMinutes(int seconds);
char*				dblSecondsToStrMinutes(double seconds);
char*				dblSecondsToStrMinutesFrames(double seconds);
FILE*				LoadSettings(int argc, char ** argv);
int					GetAvgBrightness(void);
bool				CheckFrameIsBlack(void);
void				BuildBlackFrameCommList(void);
bool				CheckSceneHasChanged(void);
#if 0
void				BuildSceneChangeCommList(void);
void				BuildSceneChangeCommList2(void);
#endif
void                backfill_frame_volumes();
void				PrintLogoFrameGroups(void);
void				PrintCCBlocks(void);
void				ResetLogoBuffers(void);
void				EdgeDetect(unsigned char* frame_ptr, int maskNumber);
void				EdgeCount(unsigned char* frame_ptr);
void				FillLogoBuffer(void);
bool				SearchForLogoEdges(void);
double				CheckStationLogoEdge(unsigned char* testFrame);
double				DoubleCheckStationLogoEdge(unsigned char* testFrame);
void				SetEdgeMaskArea(unsigned char* temp);
int					ClearEdgeMaskArea(unsigned char* temp, unsigned char* test);
int					CountEdgePixels(void);
void				DumpEdgeMask(unsigned char* buffer, int direction);
void				DumpEdgeMasks(void);
void				BuildBlackFrameAndLogoCommList(void);
bool				CheckFramesForLogo(int start, int end);
char				CheckFramesForCommercial(int start, int end);
char				CheckFramesForReffer(int start, int end);
void				SaveLogoMaskData(void);
void				LoadLogoMaskData(void);
double				CalculateLogoFraction(int start, int end);
bool				CheckFrameForLogo(int i);
int					CountSceneChanges(int StartFrame, int EndFrame);
void				Debug(int level, const char * fmt, ...);
void				InitProcessLogoTest(void);
void				InitComSkip(void);
void				InitLogoBuffers(void);
void				FindIniFile(void);
double				FindScoreThreshold(double percentile);
void				OutputLogoHistogram(int buckets);
void				OutputbrightHistogram(void);
void				OutputuniformHistogram(void);
void				OutputHistogram(int *histogram, int scale, char *title, bool truncate);
int					FindBlackThreshold(double percentile);
int					FindUniformThreshold(double percentile);
void				OutputFrameArray(bool screenOnly);
void                OutputBlackArray();
void				OutputFrame(int frame_number);
void				OpenOutputFiles();
void				InitializeFrameArray(long i);
void				InitializeBlackArray(long i);
void				InitializeSchangeArray(long i);
void				InitializeLogoBlockArray(long i);
void				InitializeARBlockArray(long i);
void				InitializeACBlockArray(long i);
void				InitializeBlockArray(long i);
void				InitializeCCBlockArray(long i);
void				InitializeCCTextArray(long i);
void				PrintArgs(void);
void        close_dump(void);
void				OutputCommercialBlock(int i, long prev, long start, long end, bool last);
void				ProcessCSV(FILE *);
void				OutputCCBlock(long i);
void				ProcessCCData(void);
bool				CheckOddParity(unsigned char ch);
void				AddNewCCBlock(long current_frame, int type, bool cc_on_screen, bool cc_in_memory);
char*				CCTypeToStr(int type);
int					DetermineCCTypeForBlock(long start, long end);
double				AverageARForBlock(int start, int end);
void				SetARofBlocks(void);
bool				ProcessCCDict(void);
int					FindBlock(long frame);
void				BuildCommListAsYouGo(void);
void				BuildCommercial(void);
int					RetreiveVolume (int f);
void InsertBlackFrame(int f, int b, int u, int v, int c);
extern void DecodeOnePicture(FILE * f, double pts);



extern "C" int CEW_init(int argc, char *argv[]);


extern int oheight;
extern int owidth;
extern double divider;
extern int oldfrm;
extern int zstart;
extern int zfactor;
extern int show_XDS;
extern int show_silence;
extern int preMarkerFrame;
extern int postMarkerFrame;
extern int shift;
extern int beforeblocks[100];
extern int afterblocks[100];
extern int length_order[2000];
extern int length_sorted;
extern int min_val[10];
extern int max_val[10];
extern int delta_val[10];
extern char TempXmlFilename[300];
extern unsigned char MPEG2SysHdr[24];
extern int own_histogram[4][256];
extern int scan_step;
extern unsigned char XDSbuffer[40][100];
extern int lastXDS;
extern int firstXDS;
extern int startXDS;
extern int baseXDS;
extern const char * ratingSystem[4];
extern FILE * dump_audio_file;
extern FILE * dump_video_file;
double get_frame_pts(int f);
char *CauseString(int i);
double ValidateBlackFrames(long reason, double ratio, int remove);
bool BuildBlocks(bool recalc);
void FindLogoThreshold();
void CleanLogoBlocks();
void InitScanLines();
void InitHasLogo();
void OutputDebugWindow(bool showVideo, int frm, int grf, bool forceRefresh);
void Recalc();
bool ReviewResult();
int DetectCommercials(int f, double pts);
int Max(int i,int j);
int Min(int i,int j);
double AverageARForBlock(int start, int end);
int AverageACForBlock(int start, int end);
double	FindARFromHistogram(double ar_ratio);
void FillARHistogram(bool refill);
void FillACHistogram(bool refill);
void InsertBlackFrame(int f, int b, int u, int v, int c);
bool BuildMasterCommList(void);
bool WithinDivisibleTolerance(double test_number, double divisor, double tolerance);
void BuildPunish();
void WeighBlocks(void);
char *EscapeXmlFilename(char *f);
void OpenOutputFiles();
void OutputCommercialBlock(int i, long prev, long start, long end, bool last);
char CompareLetter(int value, int average, int i);
void BuildCommercial();
bool OutputBlocks(void);
void OutputStrict(double len, double delta, double tol);
void OutputTraining();
bool OutputCleanMpg();
bool LengthWithinTolerance(double test_length, double expected_length, double tolerance);
bool IsStandardCommercialLength(double length, double tolerance, bool strict);
double FindNumber(char* data, const char* key, double fallback);
char* intSecondsToStrMinutes(int seconds);
char* dblSecondsToStrMinutes(double seconds);
char* dblSecondsToStrMinutesFrames(double seconds);
void LoadIniFile();
FILE* LoadSettings(int argc, char ** argv);
void ProcessARInfoInit(int minY, int maxY, int minX, int maxX);
void ProcessARInfo(int minY, int maxY, int minX, int maxX);
void ProcessACInfoInit(int audio_channels);
void ProcessACInfo(int audio_channels);
int MatchCutScene(unsigned char *cutscene);
void RecordCutScene(int frame_count, int brightness);
void LoadCutScene(const char *filename);
void ScanBottom(intptr_t arg);
void ScanTop(intptr_t arg);
void ScanLeft(intptr_t arg);
void ScanRight(intptr_t arg);
void DetectCredits(int frame_count);
bool CheckSceneHasChanged(void);
void PrintLogoFrameGroups(void);
void PrintCCBlocks(void);
void EdgeDetect(unsigned char* frame_ptr, int maskNumber);
double CheckStationLogoEdge(unsigned char* testFrame);
double DoubleCheckStationLogoEdge(unsigned char* testFrame);
void InitProcessLogoTest();
bool ProcessLogoTest(int framenum_real, int curLogoTest, int close);
void ResetLogoBuffers(void);
void FillLogoBuffer(void);
bool SearchForLogoEdges(void);
int ClearEdgeMaskArea(unsigned char* temp, unsigned char* test);
void SetEdgeMaskArea(unsigned char* temp);
int CountEdgePixels(void);
void DumpEdgeMask(unsigned char* buffer, int direction);
void DumpEdgeMasks(void);
bool CheckFramesForLogo(int start, int end);
double CalculateLogoFraction(int start, int end);
bool CheckFrameForLogo(int i);
char CheckFramesForCommercial(int start, int end);
char CheckFramesForReffer(int start, int end);
void SaveLogoMaskData(void);
void LoadLogoMaskData(void);
int CountSceneChanges(int StartFrame, int EndFrame);
void Debug(int level, const char * fmt, ...);
void InitLogoBuffers(void);
void InitComSkip(void);
void FindIniFile(void);
double FindScoreThreshold(double percentile);
void OutputLogoHistogram(int buckets);
void OutputbrightHistogram(void);
void OutputuniformHistogram(void);
void OutputHistogram(int *histogram, int scale, char *title, bool truncate);
int FindBlackThreshold(double percentile);
int FindUniformThreshold(double percentile);
void OutputFrame(int frame_number);
int FindFrameWithPts(double t);
int InputReffer(const char *extension, int setfps);
void OutputAspect(void);
void OutputBlackArray();
void OutputFrameArray(bool screenOnly);
void InitializeFrameArray(long i);
void InitializeBlackArray(long i);
void InitializeSchangeArray(long i);
void InitializeLogoBlockArray(long i);
void InitializeARBlockArray(long i);
void InitializeACBlockArray(long i);
void InitializeBlockArray(long i);
void InitializeCCBlockArray(long i);
void InitializeCCTextArray(long i);
void PrintArgs(void);
void ProcessCSV(FILE *in_file);
void OutputCCBlock(long i);
void Init_XDS_block();
void Add_XDS_block();
void AddXDS(unsigned char hi, unsigned char lo);
void AddCC(int i);
void ProcessCCData(void);
bool CheckOddParity(unsigned char ch);
void AddNewCCBlock(long current_frame, int type, bool cc_on_screen, bool cc_in_memory);
char* CCTypeToStr(int type);
int DetermineCCTypeForBlock(long start, long end);
void SetARofBlocks(void);
bool ProcessCCDict(void);
int FindBlock(long frame);
void BuildCommListAsYouGo(void);
double get_fps();
void set_fps(double fp);
void set_frame_volume(unsigned int f, int volume);
void dump_audio_start();
void dump_audio (char *start, char *end);
void dump_video_start();
void dump_video (char *start, char *end);
void close_dump(void);
void dump_data(char *start, int length);
void close_data();
