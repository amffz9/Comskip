#ifndef COMSKIP_LEGACY_DETECTION_H
#define COMSKIP_LEGACY_DETECTION_H
#pragma once
struct RecordingContext;
#include "translator.h"
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
#include "file_resources.h"
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


#include "comskip.h"
#include "commercial_length.h"
#include "settings_value.h"
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
#define DEEP_SILENCE	6	//context.settings.max_volume / DEEP_SILENCE defines deep silence

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















































extern "C" char osname[];



#define KDOWN	1
#define KUP		2
#define KLEFT	3
#define KRIGHT	4
#define KNEXT	5
#define KPREV	6










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






                        // frames per second (NTSC=29.970, PAL=25)

double get_frame_pts(RecordingContext& context, int f);

#define F2V(X) (!context.state.frame.empty() ? ((X) <= 0 ? context.state.frame[1].pts : ((X) >= context.state.framenum_real ? context.state.frame[context.state.framenum_real - 1].pts : context.state.frame[X].pts )) : (X) / context.settings.fps)
#include <cassert>
//#define F2T(X) (F2V(X) - F2V(1))
#define F2T(X) (F2V(X))
#define F2L(X,Y) (F2V(X) - F2V(Y))

#define F2F(X) ((long) (F2T(X) * context.settings.fps + 1.5 ))

typedef struct
{
    long	frame;
    int		percentage;
} schange_info;





typedef struct
{
    long	frame;
    int		brightness;
    long	uniform;
    int		volume;
    int		cause;
} black_frame_info;





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








typedef struct
{
    unsigned char	cc1[2];
    unsigned char	cc2[2];
} ccPacket;




typedef struct
{
    long	start_frame;
    long	end_frame;
    int		type;
} cc_block_info;









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






typedef struct
{
    long			start_frame;
    long			end_frame;
    long			text_len;
    unsigned char	text[256];
} cc_text_info;






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












#define AC_UNDEF	0
typedef struct
{
    int		start;
    int		end;
//	bool	ar;
    int 	audio_channels;
} ac_block_info;






typedef struct
{
    long	start;
    long	end;
} commercial_list_info;




struct Legacy_commercial_entry
{
    long	start_frame;
    long	end_frame;
    int		start_block;
    int		end_block;
    double	length;
};




struct Legacy_reffer_entry
{
    long	start_frame;
    long	end_frame;
};




#define MAX_ASPECT_RATIOS	1000
struct Legacy_ar_histogram_entry
{
    long	frames;
    double	ar_ratio;
};



#define MAX_AUDIO_CHANNELS	12
struct Legacy_ac_histogram_entry
{
    long	frames;
    int     audio_channels;
};










































//unsigned int			frame_period;
//int						audio_framenum = 0;
//extern int64_t			pts;
















#define UNIFORMSCALE 100




















//#define MAXWIDTH	2000
//#define MAXHEIGHT	1200



// unsigned char		oldframe[MAXWIDTH*MAXHEIGHT];

// variables defining options with defaults

                        // show extra info


                        // border around edge of video to ignore

                        // border from bottom to ignore

                        // border from bottom to ignore



                // frame not black if any pixels checked are greater than this (scale 0 to 255)



                // frame not pure black if any pixels are greater than this, will check average
            // maximum average brightness for a dim frame to be considered black (scale 0 to

















//int					variable_bitrate = 1;


///brightness (scale 0 to 255)

///255)
    // maximum length in seconds to consider a segment a commercial break
    // minimum length in seconds to consider a segment a commercial break
    // maximum time in seconds for a single commercial
    // mimimum time in seconds for a single commercial

                    // set=1 to only mark breaks divisible by 5 as a commercial.
















            // If no logo is identified after x seconds into the show - give up.
            // If no logo is identified after x seconds into the show - give up.























































































































// bool				frameIsBlack;









#define				MAXCSLENGTH		400*300
#define				MAXCUTSCENES	8


void LoadCutScene(RecordingContext& context, const char *filename);
void RecordCutScene(RecordingContext& context, int frame_count,int brightness);




















// int					cssum[MAXCUTSCENES];
// int					csmatch[MAXCUTSCENES];








                // How many frames to compare at a time for logo detection;

// int					logoTrendStartFrame;


// int					lastRealLogoChange;


// bool				hindsightLogoState = true;


#define MULTI_EDGE_BUFFER 0
#if MULTI_EDGE_BUFFER
unsigned char **	horiz_edges = NULL;				// rotating storage for detected horizontal edges
unsigned char **	vert_edges = NULL;					// rotating storage for detected vertical edges
#else


#endif


















































//bool				currentAR;
//bool				lastAR;
//bool				showAvgAR;








#define MAXTIMEFLAG 2




















// Function Prototypes
bool				BuildBlocks(RecordingContext& context, bool recalc);
void				Recalc(RecordingContext& context);
double				ValidateBlackFrames(RecordingContext& context, long reason, double ratio, int remove);
int					DetectCommercials(RecordingContext& context, int, double);
bool				BuildMasterCommList(RecordingContext& context);
void				WeighBlocks(RecordingContext& context);
bool				OutputBlocks(RecordingContext& context);
void        OutputAspect(RecordingContext& context);
void        OutputTraining(RecordingContext& context);
bool ProcessLogoTest(RecordingContext& context, int framenum_real, int curLogoTest, int close);
void        OutputStrict(RecordingContext& context, double len, double delta, double tol);
int					InputReffer(RecordingContext& context, const char *ext, int setfps);
bool				IsStandardCommercialLength(RecordingContext& context, double length, double tolerance, bool strict);
bool				LengthWithinTolerance(RecordingContext& context, double test_length, double expected_length, double tolerance);
double				FindNumber(RecordingContext& context, char* str1, const char* str2, double v);
char*				intSecondsToStrMinutes(RecordingContext& context, int seconds);
char*				dblSecondsToStrMinutes(RecordingContext& context, double seconds);
char*				dblSecondsToStrMinutesFrames(RecordingContext& context, double seconds);
FILE* LoadSettings(RecordingContext& context, int argc, char ** argv, const comskip::localization::Translator& translator);
int					GetAvgBrightness(void);
bool				CheckFrameIsBlack(void);
void				BuildBlackFrameCommList(void);
bool				CheckSceneHasChanged(RecordingContext& context);
#if 0
void				BuildSceneChangeCommList(void);
void				BuildSceneChangeCommList2(void);
#endif
void                backfill_frame_volumes(RecordingContext& context);
void				PrintLogoFrameGroups(RecordingContext& context);
void				PrintCCBlocks(RecordingContext& context);
void				ResetLogoBuffers(RecordingContext& context);
void				EdgeDetect(RecordingContext& context, unsigned char* frame_ptr, int maskNumber);
void				EdgeCount(unsigned char* frame_ptr);
void				FillLogoBuffer(RecordingContext& context);
bool				SearchForLogoEdges(RecordingContext& context);
double				CheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame);
double				DoubleCheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame);
void				SetEdgeMaskArea(RecordingContext& context, unsigned char* temp);
int					ClearEdgeMaskArea(RecordingContext& context, unsigned char* temp, unsigned char* test);
int					CountEdgePixels(RecordingContext& context);
void				DumpEdgeMask(RecordingContext& context, unsigned char* buffer, int direction);
void				DumpEdgeMasks(RecordingContext& context);
void				BuildBlackFrameAndLogoCommList(void);
bool				CheckFramesForLogo(RecordingContext& context, int start, int end);
char				CheckFramesForCommercial(RecordingContext& context, int start, int end);
char				CheckFramesForReffer(RecordingContext& context, int start, int end);
void				SaveLogoMaskData(RecordingContext& context);
void				LoadLogoMaskData(RecordingContext& context);
double				CalculateLogoFraction(RecordingContext& context, int start, int end);
bool				CheckFrameForLogo(RecordingContext& context, int i);
int					CountSceneChanges(RecordingContext& context, int StartFrame, int EndFrame);
void				Debug(RecordingContext& context, int level, const char * fmt, ...);
void				InitProcessLogoTest(RecordingContext& context);
void				InitComSkip(RecordingContext& context);
void				InitLogoBuffers(RecordingContext& context);
void				FindIniFile(RecordingContext& context);
double				FindScoreThreshold(RecordingContext& context, double percentile);
void				OutputLogoHistogram(RecordingContext& context, int buckets);
void				OutputbrightHistogram(RecordingContext& context);
void				OutputuniformHistogram(RecordingContext& context);
void				OutputHistogram(RecordingContext& context, int *histogram, int scale, char *title, bool truncate);
int					FindBlackThreshold(RecordingContext& context, double percentile);
int					FindUniformThreshold(RecordingContext& context, double percentile);
void				OutputFrameArray(RecordingContext& context, bool screenOnly);
void                OutputBlackArray(RecordingContext& context);
void				OutputFrame(RecordingContext& context, int frame_number);
void				OpenOutputFiles(RecordingContext& context);
void				InitializeFrameArray(RecordingContext& context, long i);
void				InitializeBlackArray(RecordingContext& context, long i);
void				InitializeSchangeArray(RecordingContext& context, long i);
void				InitializeLogoBlockArray(RecordingContext& context, long i);
void				InitializeARBlockArray(RecordingContext& context, long i);
void				InitializeACBlockArray(RecordingContext& context, long i);
void				InitializeBlockArray(RecordingContext& context, long i);
void				InitializeCCBlockArray(RecordingContext& context, long i);
void				InitializeCCTextArray(RecordingContext& context, long i);
void				PrintArgs(RecordingContext& context);
void        close_dump(RecordingContext& context);
void				OutputCommercialBlock(RecordingContext& context, int i, long prev, long start, long end, bool last);
void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input);
void				OutputCCBlock(RecordingContext& context, long i);
void				ProcessCCData(RecordingContext& context);
bool				CheckOddParity(unsigned char ch);
void				AddNewCCBlock(RecordingContext& context, long current_frame, int type, bool cc_on_screen, bool cc_in_memory);
char*				CCTypeToStr(RecordingContext& context, int type);
int					DetermineCCTypeForBlock(RecordingContext& context, long start, long end);
double				AverageARForBlock(RecordingContext& context, int start, int end);
void				SetARofBlocks(RecordingContext& context);
bool				ProcessCCDict(RecordingContext& context);
int					FindBlock(RecordingContext& context, long frame);
void				BuildCommListAsYouGo(RecordingContext& context);
void				BuildCommercial(RecordingContext& context);
int					RetreiveVolume (int f);
void InsertBlackFrame(RecordingContext& context, int f, int b, int u, int v, int c);
extern void DecodeOnePicture(RecordingContext& context, FILE * f, double pts);



extern "C" int CEW_init(int argc, char *argv[]);
































double get_frame_pts(RecordingContext& context, int f);
char *CauseString(RecordingContext& context, int i);
double ValidateBlackFrames(RecordingContext& context, long reason, double ratio, int remove);
bool BuildBlocks(RecordingContext& context, bool recalc);
void FindLogoThreshold(RecordingContext& context);
void CleanLogoBlocks(RecordingContext& context);
void InitScanLines(RecordingContext& context);
void InitHasLogo(RecordingContext& context);
void OutputDebugWindow(RecordingContext& context, bool showVideo, int frm, int grf, bool forceRefresh);
void Recalc(RecordingContext& context);
bool ReviewResult(RecordingContext& context);
int DetectCommercials(RecordingContext& context, int f, double pts);
int Max(int i,int j);
int Min(int i,int j);
double AverageARForBlock(RecordingContext& context, int start, int end);
int AverageACForBlock(RecordingContext& context, int start, int end);
double	FindARFromHistogram(RecordingContext& context, double ar_ratio);
void FillARHistogram(RecordingContext& context, bool refill);
void FillACHistogram(RecordingContext& context, bool refill);
void InsertBlackFrame(RecordingContext& context, int f, int b, int u, int v, int c);
bool BuildMasterCommList(RecordingContext& context);
bool WithinDivisibleTolerance(double test_number, double divisor, double tolerance);
void BuildPunish(RecordingContext& context);
void WeighBlocks(RecordingContext& context);

void OpenOutputFiles(RecordingContext& context);
void WriteXmlOutputFiles(RecordingContext& context, bool use_reference);
void OutputCommercialBlock(RecordingContext& context, int i, long prev, long start, long end, bool last);
char CompareLetter(RecordingContext& context, int value, int average, int i);
void BuildCommercial(RecordingContext& context);
bool OutputBlocks(RecordingContext& context);
void OutputStrict(RecordingContext& context, double len, double delta, double tol);
void OutputTraining(RecordingContext& context);
bool LengthWithinTolerance(RecordingContext& context, double test_length, double expected_length, double tolerance);
bool IsStandardCommercialLength(RecordingContext& context, double length, double tolerance, bool strict);
double FindNumber(RecordingContext& context, char* data, const char* key, double fallback);
char* intSecondsToStrMinutes(RecordingContext& context, int seconds);
char* dblSecondsToStrMinutes(RecordingContext& context, double seconds);
char* dblSecondsToStrMinutesFrames(RecordingContext& context, double seconds);
void LoadIniFile(RecordingContext& context);
void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator);
FILE* LoadSettings(RecordingContext& context, int argc, char ** argv, const comskip::localization::Translator& translator);
void ProcessARInfoInit(RecordingContext& context, int minY, int maxY, int minX, int maxX);
void ProcessARInfo(RecordingContext& context, int minY, int maxY, int minX, int maxX);
void ProcessACInfoInit(RecordingContext& context, int audio_channels);
void ProcessACInfo(RecordingContext& context, int audio_channels);
int MatchCutScene(RecordingContext& context, unsigned char *cutscene);
void RecordCutScene(RecordingContext& context, int frame_count, int brightness);
void LoadCutScene(RecordingContext& context, const char *filename);
void ScanBottom(RecordingContext& context, intptr_t arg);
void ScanTop(RecordingContext& context, intptr_t arg);
void ScanLeft(RecordingContext& context, intptr_t arg);
void ScanRight(RecordingContext& context, intptr_t arg);
void DetectCredits(RecordingContext& context, int frame_count);
bool CheckSceneHasChanged(RecordingContext& context);
void PrintLogoFrameGroups(RecordingContext& context);
void PrintCCBlocks(RecordingContext& context);
void EdgeDetect(RecordingContext& context, unsigned char* frame_ptr, int maskNumber);
double CheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame);
double DoubleCheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame);
void InitProcessLogoTest(RecordingContext& context);
bool ProcessLogoTest(RecordingContext& context, int framenum_real, int curLogoTest, int close);
void ResetLogoBuffers(RecordingContext& context);
void FillLogoBuffer(RecordingContext& context);
bool SearchForLogoEdges(RecordingContext& context);
int ClearEdgeMaskArea(RecordingContext& context, unsigned char* temp, unsigned char* test);
void SetEdgeMaskArea(RecordingContext& context, unsigned char* temp);
int CountEdgePixels(RecordingContext& context);
void DumpEdgeMask(RecordingContext& context, unsigned char* buffer, int direction);
void DumpEdgeMasks(RecordingContext& context);
bool CheckFramesForLogo(RecordingContext& context, int start, int end);
double CalculateLogoFraction(RecordingContext& context, int start, int end);
bool CheckFrameForLogo(RecordingContext& context, int i);
char CheckFramesForCommercial(RecordingContext& context, int start, int end);
char CheckFramesForReffer(RecordingContext& context, int start, int end);
void SaveLogoMaskData(RecordingContext& context);
void LoadLogoMaskData(RecordingContext& context);
int CountSceneChanges(RecordingContext& context, int StartFrame, int EndFrame);
void Debug(RecordingContext& context, int level, const char * fmt, ...);
void InitLogoBuffers(RecordingContext& context);
void InitComSkip(RecordingContext& context);
void FindIniFile(RecordingContext& context);
double FindScoreThreshold(RecordingContext& context, double percentile);
void OutputLogoHistogram(RecordingContext& context, int buckets);
void OutputbrightHistogram(RecordingContext& context);
void OutputuniformHistogram(RecordingContext& context);
void OutputHistogram(RecordingContext& context, int *histogram, int scale, char *title, bool truncate);
int FindBlackThreshold(RecordingContext& context, double percentile);
int FindUniformThreshold(RecordingContext& context, double percentile);
void OutputFrame(RecordingContext& context, int frame_number);
int FindFrameWithPts(RecordingContext& context, double t);
int InputReffer(RecordingContext& context, const char *extension, int setfps);
void OutputAspect(RecordingContext& context);
void OutputBlackArray(RecordingContext& context);
void OutputFrameArray(RecordingContext& context, bool screenOnly);
void InitializeFrameArray(RecordingContext& context, long i);
void InitializeBlackArray(RecordingContext& context, long i);
void InitializeSchangeArray(RecordingContext& context, long i);
void InitializeLogoBlockArray(RecordingContext& context, long i);
void InitializeARBlockArray(RecordingContext& context, long i);
void InitializeACBlockArray(RecordingContext& context, long i);
void InitializeBlockArray(RecordingContext& context, long i);
void InitializeCCBlockArray(RecordingContext& context, long i);
void InitializeCCTextArray(RecordingContext& context, long i);
void PrintArgs(RecordingContext& context);
void OutputCCBlock(RecordingContext& context, long i);
void Init_XDS_block(RecordingContext& context);
void Add_XDS_block(RecordingContext& context);
void AddXDS(RecordingContext& context, unsigned char hi, unsigned char lo);
void AddCC(RecordingContext& context, int i);
void ProcessCCData(RecordingContext& context);
bool CheckOddParity(unsigned char ch);
void AddNewCCBlock(RecordingContext& context, long current_frame, int type, bool cc_on_screen, bool cc_in_memory);
char* CCTypeToStr(RecordingContext& context, int type);
int DetermineCCTypeForBlock(RecordingContext& context, long start, long end);
void SetARofBlocks(RecordingContext& context);
bool ProcessCCDict(RecordingContext& context);
int FindBlock(RecordingContext& context, long frame);
void BuildCommListAsYouGo(RecordingContext& context);
double get_fps(RecordingContext& context);
void set_fps(RecordingContext& context, double fp);
void set_frame_volume(RecordingContext& context, unsigned int f, int volume);
void dump_audio_start(RecordingContext& context);
void dump_audio (RecordingContext& context, char *start, char *end);
void dump_video_start(RecordingContext& context);
void dump_video (RecordingContext& context, char *start, char *end);
void close_dump(RecordingContext& context);
void dump_data(RecordingContext& context, char *start, int length);
void close_data(RecordingContext& context);

#include "recording_context.h"

#endif // COMSKIP_LEGACY_DETECTION_H
