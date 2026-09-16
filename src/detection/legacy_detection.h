#ifndef COMSKIP_LEGACY_DETECTION_H
#define COMSKIP_LEGACY_DETECTION_H
#pragma once
struct RecordingContext;
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "translator.h"
#include "scan_geometry.h"
#include "detector_records.h"
#include "storage.h"
#include "caption_observations.h"
#include "cutlist_exports.h"
#include "output/diagnostics.h"
#include "output/media_dump.h"
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




#undef FRAME_WITH_HISTOGRAM
#undef FRAME_WITH_LOGO
#undef FRAME_WITH_AR

                        // frames per second (NTSC=29.970, PAL=25)

double get_frame_pts(RecordingContext& context, int f);

#define F2V(X) (!context.state.frame.empty() ? ((X) <= 0 ? context.state.frame[1].pts : ((X) >= context.state.framenum_real ? context.state.frame[context.state.framenum_real - 1].pts : context.state.frame[X].pts )) : (X) / context.settings.fps)
#include <cassert>
//#define F2T(X) (F2V(X) - F2V(1))
#define F2T(X) (F2V(X))
#define F2L(X,Y) (F2V(X) - F2V(Y))

#define F2F(X) ((long) (F2T(X) * context.settings.fps + 1.5 ))


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

#define AR_UNDEF	0.0

#define AC_UNDEF	0

#define MAX_ASPECT_RATIOS	1000

#define MAX_AUDIO_CHANNELS	12

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


bool ProcessLogoTest(RecordingContext& context, int framenum_real, int curLogoTest, int close);

bool				IsStandardCommercialLength(RecordingContext& context, double length, double tolerance, bool strict);
bool				LengthWithinTolerance(RecordingContext& context, double test_length, double expected_length, double tolerance);
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

void				PrintArgs(RecordingContext& context);

void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input);






double				AverageARForBlock(RecordingContext& context, int start, int end);
void				SetARofBlocks(RecordingContext& context);

int					FindBlock(RecordingContext& context, long frame);
void				BuildCommListAsYouGo(RecordingContext& context);

void InsertBlackFrame(RecordingContext& context, int f, int b, int u, int v, int c);


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


void WriteXmlOutputFiles(RecordingContext& context, bool use_reference);

char CompareLetter(RecordingContext& context, int value, int average, int i);




bool LengthWithinTolerance(RecordingContext& context, double test_length, double expected_length, double tolerance);
bool IsStandardCommercialLength(RecordingContext& context, double length, double tolerance, bool strict);
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
void PrintArgs(RecordingContext& context);










void SetARofBlocks(RecordingContext& context);

int FindBlock(RecordingContext& context, long frame);
void BuildCommListAsYouGo(RecordingContext& context);
double get_fps(RecordingContext& context);
void set_fps(RecordingContext& context, double fp);
void set_frame_volume(RecordingContext& context, unsigned int f, int volume);

#include "recording_context.h"

#endif // COMSKIP_LEGACY_DETECTION_H
