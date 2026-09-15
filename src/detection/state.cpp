#include "legacy_detection.h"





FILE*			out_file = NULL;




































int audio_channels;




long				frame_count = 0;













bool					processCC = false;





































int                     use_cuvid = 0;
int                     use_vdpau = 0;
int                     use_dxva2 = 0;
int                     use_qsv = 0;
int						dvrms_live_tv_retries = 300;
int						standoff = 0;




char					mpegfilename[MAX_PATH];

char					inbasename[MAX_PATH];














char					HomeDir[256];
























int					selftest = 0;











bool				output_console = true;















unsigned char*		frame_ptr = 0;




bool				lastFrameWasSceneChange = false;














































uint8_t				ccData[500];
int					ccDataLen;












long				lastFrameCommCalculated = 0;















double get_frame_pts(RecordingContext& context, int f) {
    if (!context.state.frame) {
            return(f / context.settings.fps);
    }
    if (f < 1)
        f = 1;
    if (f > context.state.frame_count -1)
        f = context.state.frame_count -1;
    return(context.state.frame[f].pts);
}
