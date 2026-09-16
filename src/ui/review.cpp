#include "platform/utf8_paths.h"
#include "exit_requested.h"
#include "media/decoder.h"
#include "output/ffmpeg_sidecar_adapter.h"
#include "output/frame_script_adapter.h"
#include "output/player_export_adapter.h"
#include "legacy_detection.h"
#include "checked_format.h"
#include "review_messages.h"
#include "image_geometry.h"
#include "review_media.h"
#include "review_intervals.h"
#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <type_traits>
#include <vector>












void OutputDebugWindow(RecordingContext& context, bool showVideo, int frm, int grf, bool forceRefresh)
{

    int i,j,x,y,a=0,c=0,r,s=0,g,gc,lb=0,e=0,n=0,bl,xd;
    int v,w;
    int bartop = 0;
    int b,cb;
    int barh = 32;
    constexpr int plot_count = 9;
    const auto set_pixel = [&](int pixel_x, int pixel_y, int red, int green, int blue) {
        if (pixel_x < 0 || pixel_x >= context.state.owidth || pixel_y < 0 || pixel_y >= context.state.oheight + barh)
            return;
        const auto offset = 3 * (static_cast<std::size_t>(pixel_y) * context.state.owidth + pixel_x);
        context.state.graph[offset] = static_cast<unsigned char>(std::clamp(red, 0, 255));
        context.state.graph[offset + 1] = static_cast<unsigned char>(std::clamp(green, 0, 255));
        context.state.graph[offset + 2] = static_cast<unsigned char>(std::clamp(blue, 0, 255));
    };
    const auto gray_pixel = [&](int pixel_x, int pixel_y, int level) {
        set_pixel(pixel_x, pixel_y, level, level, level);
    };
    const auto plot = [&](int plots, int slot, int position, double value, double maximum, double threshold,
                          int red, int green, int blue) {
        if (plots <= 0 || maximum <= 0) return;
        const auto height = context.state.oheight / plots;
        if (height <= 0) return;
        const auto level = static_cast<int>(std::clamp(value * (height - 5) / maximum, 0.0, static_cast<double>(height - 1)));
        const auto y = context.state.oheight - height * slot - level;
        set_pixel(position, y, value < threshold ? 255 : red, value < threshold ? 255 : green,
                  value < threshold ? 255 : blue);
    };
    char t[1024];
    bool	blackframe, bothtrue, haslogo, uniformframe;
    int silence=0;
//	frm++;
    if (!forceRefresh && context.state.oldfrm == frm && context.state.review_source_width == context.state.videowidth && context.state.review_source_height == context.state.height)
        return;
    context.state.oldfrm = frm;
    if (context.settings.output_debugwindow && context.state.frame_count )
    {

        if (!context.window.is_open() || context.state.review_source_width != context.state.videowidth || context.state.review_source_height != context.state.height)
        {
            if (context.state.width == 0 /*|| (loadingCSV && !showVideo) */)
                context.state.videowidth = context.state.width = 800; // MAXWIDTH;
            if (context.state.height == 0 /*||  (loadingCSV && !showVideo) */)
                context.state.height = 600-barh; // MAXHEIGHT-30;
            if (context.settings.edge_step == 0) {
                context.settings.edge_step = 1;
            }
            if (context.state.height > 600 || context.state.width > 800)
            {
                context.state.oheight = context.state.height / 2;
                context.state.owidth = context.state.width / 2 ;
                context.state.owidth = context.state.videowidth / 2 ;
                context.state.divider = 2;
            }
            else
            if (context.state.height < 150 || context.state.width < 200)
            {
                context.state.divider = 0.25;
                context.state.oheight = context.state.height / context.state.divider;
                context.state.owidth = context.state.width / context.state.divider;
                context.state.owidth = context.state.videowidth / context.state.divider;
            } else
            if (context.state.height < 300 || context.state.width < 400)
            {
                context.state.divider = 0.5;
                context.state.oheight = context.state.height / context.state.divider;
                context.state.owidth = context.state.width / context.state.divider;
                context.state.owidth = context.state.videowidth / context.state.divider;
            } else
            {
                context.state.oheight = context.state.height;
                context.state.owidth = context.state.width;
                context.state.owidth = context.state.videowidth;
                context.state.divider = 1;
            }
            context.state.oheight = (context.state.oheight + 31) & -32;
            context.state.owidth = (context.state.owidth + 31) & -32;
            std::string title;
            comskip::checked_format(title, context.settings.windowtitle.c_str(), context.state.filename);
            context.window.close();
            context.state.ensure_review_graph(context.state.owidth, context.state.oheight + barh);
            context.window.close();
            context.window.configure_font(comskip::platform::path_from_utf8(context.settings.review_font_file),
                context.settings.review_font_size);
            context.window.open(context.state.owidth, context.state.oheight + barh, title);
            context.state.review_source_width = context.state.videowidth;
            context.state.review_source_height = context.state.height;

        }
//		bartop = context.state.oheight;
        if (frm >= context.state.frame_count)
            frm = context.state.frame_count-1;
        if (frm < 1)
            frm = 1;

        v = context.state.frame_count/context.state.zfactor;

        if ( frm < context.state.zstart + v / 10) context.state.zstart = frm - v / 10;
        if ( context.state.zstart < 0 ) context.state.zstart = 0;


        if ( frm > v + context.state.zstart - v / 10) context.state.zstart = frm - v + v / 10;

        if (context.state.zstart + v > context.state.frame_count) context.state.zstart = context.state.frame_count - v;
//		if ( frm > v + zstart) zstart = frm - v;

        w = ((frm - context.state.zstart)* context.state.owidth / v);



        if (showVideo && context.state.frame_ptr)
        {
            const comskip::detection::LumaImageView luma(
                std::span{context.state.frame_ptr, comskip::detection::checked_image_size(context.state.width, context.state.height)},
                context.state.width, context.state.videowidth, context.state.height);
            std::ranges::fill(context.state.graph, 0);
            /*
                        for (x = 0; x < border; x++) {
                            for (y = 0; y < context.state.oheight; y++) {
                                gray_pixel(x,y+barh, 0);
                                gray_pixel(context.state.owidth - 1 - x,y+barh, 0);
                            }
                        }
            */
            for (x = 0+context.settings.border; x < context.state.owidth-context.settings.border; x++)
            {
//				for (y = 0; y < border; y++) {
//					gray_pixel(x,y+barh, 0);
//					gray_pixel(x,context.state.oheight - 1 - (y+barh), 0);
//				}
                for (y = 0+context.settings.border; y < context.state.oheight-context.settings.border; y++)
                {
                    if (x*context.state.divider < context.state.width && y*context.state.divider < context.state.height)
                        gray_pixel(x, y + barh, luma.scaled_sample(x, y, context.state.divider) >> (grf ? 1 : 0));
//					gray_pixel(x,y+barh, min_br[(y*context.state.divider)*context.state.width+(x*context.state.divider)]);		//MAXMIN Logo search

//					gray_pixel(x,y+barh, vert_edges[(y*context.state.divider)*context.state.width+(x*context.state.divider)]);	//Edge detect

//					gray_pixel(x,y+barh, (ver_edgecount[(y*context.state.divider)*context.state.width+(x*context.state.divider)]* 4));		// Edge count
                    /*
                                        gray_pixel(x,y+barh, (abs((frame_ptr[(y*context.state.divider)*context.state.width+(x*context.state.divider)] +
                                                                  frame_ptr[(y*context.state.divider)*context.state.width+((x+1)*context.state.divider)])/2
                                                                  -
                                                                  (frame_ptr[(y*context.state.divider)*context.state.width+((x+2)*context.state.divider)]+
                                                                  frame_ptr[(y*context.state.divider)*context.state.width+((x+3)*context.state.divider)])/2
                                                            ) > edge_level_threshold ? 200 : 0));
                    */
                    //					context.state.graph[((context.state.oheight - y)*context.state.owidth+x)*3+0] = frame_ptr[y*context.state.owidth+x];
                    //					context.state.graph[((context.state.oheight - y)*context.state.owidth+x)*3+1] = frame_ptr[y*context.state.owidth+x];
                    //					context.state.graph[((context.state.oheight - y)*context.state.owidth+x)*3+2] = frame_ptr[y*context.state.owidth+x];
                }
            }
            //			memcpy(&context.state.graph[context.state.owidth*context.state.oheight * 0], frame_ptr, context.state.owidth*context.state.oheight);
            //			memcpy(&context.state.graph[context.state.owidth*context.state.oheight * 1], frame_ptr, context.state.owidth*context.state.oheight);
            //			memcpy(&context.state.graph[context.state.owidth*context.state.oheight * 2], frame_ptr, context.state.owidth*context.state.oheight);
            if (context.state.framearray && grf && ((context.settings.commDetectMethod & LOGO) || context.state.logoInfoAvailable ))
            {
                if (context.settings.aggressive_logo_rejection)
                    s = context.settings.edge_radius/2;				// Cater of mask offset
                else
                    s = 0;
//				w = 0;
//				v = 0;
                if (context.state.logoInfoAvailable)  	// Show logo mask
                {
                    if (context.state.frame[frm].currentGoodEdge > context.settings.logo_threshold)
                    {
                        e = (int)(context.state.frame[frm].currentGoodEdge * 250);
                        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY ; y += context.settings.edge_step)
                        {
                            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX ; x += context.settings.edge_step)
                            {
                                if (context.state.choriz_edgemask[y * context.state.width + x]) r = 255;
                                else r = 0;
                                if (context.state.cvert_edgemask[y * context.state.width + x]) g = 255;
                                else g = 0;
                                if (r || g) set_pixel(((int)((x-s)/context.state.divider)),((int)((y-s)/context.state.divider))+barh,r,g,0);
                            }
                        }
                    }
                }
                else  					// Show detected logo pixels only while scanning input
                {
                    if (frm+1 == context.state.frame_count)
                    {

                        for (x = (context.settings.logo_at_side ? context.state.width / 2 : context.settings.edge_radius + context.settings.border + 4 * context.settings.edge_step); x < context.state.videowidth - context.settings.edge_radius - context.settings.border - 4 * context.settings.edge_step; x = (x == context.state.videowidth / 3 ? 2 * context.state.videowidth / 3 : x + context.settings.edge_step))
                        {
                            for (y = (context.settings.logo_at_bottom ? context.state.height / 2 : context.settings.edge_radius + context.settings.border + 4 * context.settings.edge_step); y < (context.settings.subtitles ? context.state.height / 2 : context.state.height - context.settings.edge_radius - context.settings.border - 4 * context.settings.edge_step); y = (y == context.state.height / 3 ? 2 * context.state.height / 3 : y + context.settings.edge_step))
                            {
                                if (context.state.edgemask_filled) {
                                    r = 255 * context.state.thoriz_edgemask[(y) * context.state.width + (x)];
                                    g = 255 * context.state.tvert_edgemask[(y) * context.state.width + (x)];
                                } else {
                                    r = 255 * context.state.hor_edgecount[(y) * context.state.width + (x)] / context.settings.num_logo_buffers;
                                    g = 255 * context.state.ver_edgecount[(y) * context.state.width + (x)] / context.settings.num_logo_buffers;
                                }
                                if (r > 255) r = 255;
                                if (g > 255) g = 255;
                                //if (r > 128 || g >  128)
                                    set_pixel(((int)(x/context.state.divider)),((int)(y/context.state.divider))+barh,r,g,0);

}
                        }

                    }

                }

            }
        }
        else
        {
            std::ranges::fill(context.state.graph, 0);

        }


        if (context.state.framearray && grf)
        {
            for (y=0; y < context.state.oheight; y++)
            {
                set_pixel(w,y,100,100,100);
            }
            bl = 0;
            for (x=0 ; x < context.state.owidth; x++)  				// debug bar
            {
                a = 0;
                b = 0;
                s = 0;
                c = 0;
                n = 0;
                if (context.state.block_count && grf == 2)
                {
                    while (bl < context.state.block_count && context.state.cblock[bl].f_end < context.state.zstart+(int)((double)x * v /context.state.owidth))
                        bl++;

                    plot(plot_count, 0, x, context.state.cblock[bl].brightness, 2550, (int)(context.state.avg_brightness*context.settings.punish_threshold), 0, 255, 0); // RED
                    plot(plot_count, 1, x, context.state.cblock[bl].volume/100, 100000, (int)(context.state.avg_volume*context.settings.punish_threshold)/100, 255, 0, 0); // Green
                    plot(plot_count, 2, x, context.state.cblock[bl].uniform, 3000, (int)(context.state.avg_uniform*context.settings.punish_threshold), 255, 0,0); // RED
                    plot(plot_count, 3, x, (int)(context.state.cblock[bl].schange_rate*1000), 1000, (int)(context.state.avg_schange*context.settings.punish_threshold*1000), 255, 0, 0);	// PURPLE
                }
                for (i = context.state.zstart+(int)((double)x * v /context.state.owidth); i < context.state.zstart+(int)((double)(x+1) * v /context.state.owidth ); i++)
                {
                    if (i <= context.state.frame_count)
                    {
                        b += context.state.frame[i].brightness;
                        plot(plot_count, 0, x, context.state.frame[i].brightness, 255, context.settings.max_avg_brightness, 255, 0,0); // RED
                        s += context.state.frame[i].volume;
                        plot(plot_count, 1, x, context.state.frame[i].volume, 10000, context.settings.max_volume, (context.state.frame[i].audio_channels * 40), 255, 0);			// GREEN
                        e += context.state.frame[i].uniform;
                        plot(plot_count, 2, x, context.state.frame[i].uniform, 30000, context.settings.non_uniformity, 0, 255, 255);	// LIGHT BLUE
                        c += (int)(context.state.frame[i].currentGoodEdge*100);
                        plot(plot_count, 4, x, (int)(context.state.frame[i].currentGoodEdge*100), 100, 0, 255, 255, 0);  // YELLOW
                        plot(plot_count, 4, x, (int)(context.state.frame[i].logo_filter*50+50), 100, 0, (context.state.frame[i].logo_filter < 0.0 ?255:0) , (context.state.frame[i].logo_filter < 0.0 ?0:255), 0);
                        plot(plot_count, 5, x, (int)((context.state.frame[i].ar_ratio-0.5) * 100), 250, 0, 0, 0, 255);   // BLUE

                        if (context.settings.commDetectMethod & CUTSCENE)
                        {
                            plot(plot_count, 3, x, (int)(context.state.frame[i].cutscenematch), 100, context.settings.cutscenedelta, 255, 0, 255);     // PURPLE
                        }
                        else
                        {
                            plot(plot_count, 3, x, (int)(context.state.frame[i].schange_percent), 100, context.state.schange_cutlevel, 255, 0, 255);	    // PURPLE
                        }
                        a += context.state.frame[i].maxY;
                        plot(plot_count, 6, x, context.state.frame[i].maxY, context.state.height, 0, 0, 128, 128);
                        b += context.state.frame[i].minY;
                        plot(plot_count, 6, x, context.state.frame[i].minY, context.state.height, 0, 0, 128, 128);
                        n++;
                        plot(plot_count, 7, x, context.state.frame[i].maxX, context.state.width, 0, 0, 0, 255);
                        plot(plot_count, 7, x, context.state.frame[i].minX, context.state.width, 0, 0, 0, 255);
                    }
                }
                if (n > 0)
                {
                    a /= n;
//						f /= n;
                    b /= n;
                    s /= n;
                    c /= n;
                    e /= n;
                }
// plot(S, I, X, Y, MAX, L, R,G,B)
            }
        }



        if (context.state.frame_ptr && context.state.framearray)
        {
//			for (x=0; x < context.state.owidth; x++) { // Edge counter indicator
//				context.state.graph[2* context.state.owidth + x] = (x < edge_count /8 ? 255 : 0);
//			}

            x = context.state.frame[frm].maxX/context.state.divider;
            if (x == 0) x = context.state.owidth;
            for (i=context.state.frame[frm].minX/context.state.divider; i < x; i++)  				// AR lines
            {
                set_pixel(i, ((int)((context.state.frame[frm].minY/context.state.divider)))+barh, 0,0,255);
                set_pixel(i, ((int)((context.state.frame[frm].maxY/context.state.divider)+barh)), 0,0,255);

//				context.state.graph[context.state.frame[frm].minY* context.state.owidth + i] = 255;
//				context.state.graph[context.state.frame[frm].maxY* context.state.owidth + i] = 255;
            }
            for (i=(context.state.frame[frm].minY/context.state.divider); i < (context.state.frame[frm].maxY/context.state.divider); i++)  				// AR lines
            {
                set_pixel(((int)((context.state.frame[frm].minX/context.state.divider))), i+barh, 0,0,255);
                set_pixel(((int)((context.state.frame[frm].maxX/context.state.divider))), i+barh, 0,0,255);
            }


        }
        if (context.state.framearray /* && commDetectMethod & LOGO */ )
        {
for (x = context.state.tlogoMinX/context.state.divider; x < context.state.tlogoMaxX/context.state.divider; x++)  		// Logo box X
            {
                set_pixel(x,((int)(context.state.tlogoMinY/context.state.divider))+barh,255,e,e);
                set_pixel(x,((int)(context.state.tlogoMaxY/context.state.divider))+barh,255,e,e);
            }
            for (y = context.state.tlogoMinY/context.state.divider; y < context.state.tlogoMaxY/context.state.divider; y++)  		// Logo box Y
            {
                set_pixel(((int)(context.state.tlogoMinX/context.state.divider)),y+barh,255,e,e);
                set_pixel(((int)(context.state.tlogoMaxX/context.state.divider)),y+barh,255,e,e);
            }

        }

        b = 0;
        for (i = 0; i < context.state.block_count; i++)
        {
            if (context.state.cblock[i].f_start <= frm && frm <= context.state.cblock[i].f_end)
            {
                b = i;
                break;
            }
        }

        /*
                std::ranges::fill(context.state.graph, 20);
                for (i=0; i<context.state.oheight/2;i++) {
                    context.state.graph[(i*(context.state.owidth+0))*3] = 255;
                    context.state.graph[(i*(context.state.owidth+0))*3+1] = 0;
                    context.state.graph[(i*(context.state.owidth+0))*3+2] = 0;
                }
        */
//		if (0)	// disable debug bar
        for (x=0 ; x < context.state.owidth; x++)  				// debug bar
        {
            blackframe = false;
            uniformframe = false;
            silence = 0;
            haslogo = false;
            bothtrue = false;
            g = 0;
            gc = 0;
            xd = 0;
//			v = max(frame_count, DEBUGFRAMES);
            if (context.state.framearray)
            {
                xd = context.state.XDS_block_count-1;
                while (xd > 0 && context.state.XDS_block[xd].frame > context.state.zstart+(int)((double)(x+1) * v /context.state.owidth) )
                    xd--;
                if (!(xd > 0 && context.state.XDS_block[xd].frame >= context.state.zstart+(int)((double)x * v /context.state.owidth)))
                    xd = 0;

                for (i = context.state.zstart+(int)((double)x * v /context.state.owidth); i < context.state.zstart+(int)((double)(x+1) * v /context.state.owidth ); i++)
                {
                    if (i <= context.state.frame_count)
                    {
                        if ((context.state.frame[i].isblack & C_b) || (context.state.frame[i].isblack & C_r))
                        {
                            blackframe = true;
                            for (j = 0; j < context.state.block_count; j++)
                            {
                                if (context.state.cblock[j].f_end == i)
                                    bothtrue = true;
                            }
                        }
                        if (context.state.frame[i].isblack & C_u)
                        {
                            uniformframe = true;
                        }
                        if (context.state.frame[i].volume < context.settings.max_volume && silence < 1) silence = 1;
                        if ((context.state.frame[i].volume < 50 || context.state.frame[i].volume < context.settings.max_silence) && silence < 2) silence = 2;
                        if (context.state.frame[i].volume < 9) silence = 3;
                        if (context.state.frame[i].volume < 9) silence = 3;
                        if ((context.state.frame[i].isblack & C_v)) silence = 3;
                        if (context.state.frame[i].volume == 0) silence = 4;
                        if ((context.state.frame[i].isblack & C_b) && context.state.frame[i].volume < context.settings.max_volume) bothtrue = true;
                        if ((context.state.frame[i].isblack & C_r)) bothtrue = true;
                        if (frm+1 == context.state.frame_count)  						// Show details of logo while scanning
                        {
                            if (context.state.frame[i].logo_present) haslogo = true;
                        }
                        else
                        {
                            while (lb < context.state.logo_block_count && i > context.state.logo_block[lb].end)			// Show logo blocks when finished
                                lb++;
                            if (lb < context.state.logo_block_count && i >= context.state.logo_block[lb].start)
                            {
                                haslogo=true;
                            }
                        }
//					if (context.state.frame[i].currentGoodEdge > logo_threshold) haslogo = true;
                        a = (int)((context.state.frame[i].ar_ratio - 0.5 - 0.1)*6);		// Position of AR line
                        g += (int)(context.state.frame[i].currentGoodEdge * 5);
                        gc++;
                    }
                }
            }
            if (gc > 0)
                g /= gc;
            c = 255;
            if (c == 255)
            {
                for (i = 0; i <= context.state.commercial_count; i++)  	// Inside commercial?
                {
                    if (context.state.zstart+(int)((double)x * v /context.state.owidth ) >= context.state.commercial[i].start_frame &&
                            context.state.zstart+(int)((double)x * v /context.state.owidth ) <= context.state.commercial[i].end_frame )
                    {
                        c = 128;
                        break;
                    }
                }
            }

            if (c == 255)  									// not in a commercial but score above threshold
            {
                for (i = 0; i < context.state.block_count; i++)
                {
                    if (context.state.zstart+(int)((double)x * v /context.state.owidth ) >= context.state.cblock[i].f_start &&
                            context.state.zstart+(int)((double)x * v /context.state.owidth ) <= context.state.cblock[i].f_end &&
                            context.state.cblock[i].score > context.settings.global_threshold )
                    {
                        c = 220;
                        break;
                    }
                }
            }


            r = 255;
            for (i = 0; i <= context.state.reffer_count; i++)  		// Inside reference?
            {
                if (context.state.zstart+(int)((double)x * v /context.state.owidth ) >= context.state.reffer[i].start_frame &&
                        context.state.zstart+(int)((double)x * v /context.state.owidth ) <= context.state.reffer[i].end_frame )
                {
                    r = 0;
                    break;
                }
            }

            a = bartop + 14 - a;
            for (y = bartop+5; y < bartop+15 ; y++)			// Commercial / AR bar
                if (y == a)
                {
                    set_pixel(x,y,0,0,255);
                }
                else
                {
                    set_pixel(x,y,c,c,c);
//					gray_pixel(x,y, c);
                }
            g = 5; // Disable goodEdge context.state.graph
            for (i = 0; i < context.state.block_count; i++)
            {
                if (context.state.zstart+(int)((double)x * v /context.state.owidth ) >= context.state.cblock[i].f_start &&
                        context.state.zstart+(int)((double)x * v /context.state.owidth ) <= context.state.cblock[i].f_end &&
                        context.state.cblock[i].correlation > 0 )  					// if inside a correlated context.state.cblock
                {
                    g=2;
                    break;
                }
            }
            for (y = bartop + 15; y < bartop+20 ; y++)  		// Logo bar
            {

                if (haslogo) gray_pixel(x,y, ((y - (bartop + 15) == g)?255:((context.settings.commDetectMethod & LOGO)? 0 : 128)));
                else gray_pixel(x,y, ((y - (bartop + 15) == g)?0:255));
//				if (y - (bartop + 15) == g) context.state.graph[y * context.state.owidth + x] = 128;
            }

            cb = 255;
            if (context.state.block_count && context.state.cblock[b].f_start <= context.state.zstart+(int)((double)x * v /context.state.owidth ) && context.state.zstart+(int)((double)x * v /context.state.owidth ) <= context.state.cblock[b].f_end)
                cb = 0;

            if (bothtrue)
                c = 0;
            else
                c = 255;
            for (y = bartop + 20; y < bartop+25 ; y++)      // Blackframe bar
            {
                if (blackframe)
                {
                    set_pixel(x,y,c,0,0);
                }
                else if (uniformframe)
                {
                    set_pixel(x,y,0,0,c);
                }
                else
                {
                    set_pixel(x,y,255,cb,255);
                }
            }
            c = 255;

            for (y = bartop + 25; y < bartop+30 ; y++)  	// Silence bar
            {
                if (silence == 1)
                {
                    set_pixel(x,y,0,c,0);
                }
                else if (silence == 2)
                {
                    set_pixel(x,y,0,0,c);
                }
                else if (silence == 3)
                {
                    set_pixel(x,y,c,0,0);
                }
                else if (silence == 4)
                {
                    set_pixel(x,y,c,c,0);
                }
                else
                    gray_pixel(x,y, 255);
            }
            if (w < context.state.owidth)									// Progress indicator
                for (y = bartop; y < bartop+30 ; y++) set_pixel(w,y,255,0,0);

            if (context.state.preMarkerFrame > 0)
            {
                int showMkrX = ((context.state.preMarkerFrame - context.state.zstart)* context.state.owidth / v);
                for (y = bartop; y < bartop+30 ; y++) set_pixel(showMkrX,y,0,255,0);
            }

            if (context.state.postMarkerFrame > 0)
            {
                int comMkrX = ((context.state.postMarkerFrame - context.state.zstart)* context.state.owidth / v);
                for (y = bartop; y < bartop+30 ; y++) set_pixel(comMkrX,y,0,0,255);
            }

            for (y = bartop; y < bartop+(context.state.loadingTXT?20:5) ; y++)
            {
                // Reference bar
                if (xd)
                {
                    set_pixel(x,y,128,128,128);
                }
                else if (context.state.reffer_count >= 0) gray_pixel(x,y, r);
            }
        }
        context.window.draw(std::span{context.state.graph}.first(static_cast<std::size_t>(context.state.owidth) * (context.state.oheight + barh) * 3));

        //		sprintf(t, "%8i %8i %1s %1s", frm, framenum_infer, (context.state.frame[frm].isblack?"B":" "), (context.state.frame[frm].volume<context.settings.max_volume?"S":" "));
        b = 0;
        for (i = 0; i < context.state.block_count; i++)
        {
            if (context.state.cblock[i].f_start <= frm && frm <= context.state.cblock[i].f_end)
            {
                b = i;
                break;
            }
        }
        std::string frame_text;
        if (context.state.timeflag == 2 && context.state.framearray)
            frame_text = std::format("{:8.2f}", get_frame_pts(context, frm));
        else if (context.state.timeflag == 1 && context.state.framearray)
            frame_text = dblSecondsToStrMinutes(context, get_frame_pts(context, frm));
        else frame_text = std::format("{:8}", frm);

        std::string details;
        if (context.state.recalculate) {
            details = context.translator.format("review_thresholds", context.settings.max_volume,
                                                context.settings.non_uniformity, context.settings.max_avg_brightness);
        } else if (context.state.framearray) {
            const auto& analyzed = context.state.frame[frm];
            const auto brightness_flag = analyzed.isblack & C_b ? "B" : " ";
            const auto silence_flag = analyzed.volume < context.settings.max_volume ? "S" : " ";
            const auto uniform_flag = analyzed.uniform < context.settings.non_uniformity ? "U" : " ";
            const auto aspect = std::format("{:.2f}", analyzed.ar_ratio);
            if (b < context.state.block_count) {
                const auto& block = context.state.cblock[b];
                details = context.translator.format("review_frame_block", frame_text,
                    analyzed.brightness, brightness_flag, analyzed.volume, silence_flag,
                    analyzed.uniform, uniform_flag, aspect, b, std::format("{:.2f}", block.length),
                    std::format("{:.2f}", block.score), std::format("{:.2f}", block.logo),
                    CauseString(context, block.cause));
            } else {
                details = context.translator.format("review_frame", frame_text,
                    analyzed.brightness, brightness_flag, analyzed.volume, silence_flag,
                    analyzed.uniform, uniform_flag, aspect);
            }
        } else details = frame_text;

        if (context.state.soft_seeking) {
            const std::array<std::string_view, 2> lines{details, context.translator.text("review_seeking_warning")};
            context.window.show_help(lines);
        } else if (context.state.helpflag) {
            const auto translated = comskip::localization::review_help(context.translator);
            std::vector<std::string_view> lines;
            for (const auto* line : translated) if (line) lines.emplace_back(line);
            context.window.show_help(lines);
        } else if (context.state.show_XDS && context.state.XDS_block_count) {
            i = context.state.XDS_block_count - 1;
            while (i > 0 && context.state.XDS_block[i].frame > frm) --i;
            const auto& program = context.state.XDS_block[i];
            const auto duration = std::format("{:2}:{:02}", (program.duration & 0x3f00) / 256, (program.duration & 0x3f) % 256);
            const auto position = std::format("{:2}:{:02}", (program.position & 0x3f00) / 256, (program.position & 0x3f) % 256);
            const auto composite = std::format("{:2}:{:02}, {:2}/{:2}", (program.composite1 & 0x3f00) / 256,
                (program.composite1 & 0x1f) % 256, (program.composite2 & 0x1f00) / 256, (program.composite2 & 0x0f) % 256);
            const std::array<std::string, 6> text{details,
                context.translator.format("review_program_name", program.name),
                context.translator.format("review_program_rating", std::format("{:4x}", program.v_chip)),
                context.translator.format("review_program_duration", duration),
                context.translator.format("review_program_position", position),
                context.translator.format("review_composite_packet", composite)};
            std::array<std::string_view, 6> lines;
            std::ranges::transform(text, lines.begin(), [](const auto& line) { return std::string_view(line); });
            context.window.show_help(lines);
        } else if (context.state.show_silence) {
            std::array<std::string, 25> text;
            std::array<std::string_view, 25> lines;
            for (std::size_t index = 0; index < text.size(); ++index) {
                text[index] = context.translator.format("review_volume_bin", index, context.state.silenceHistogram[index]);
                lines[index] = text[index];
            }
            context.window.show_help(lines);
        } else context.window.show_details(details);
    }
    if (context.window.input().key == 0x20)
    {
        context.state.subsample_video  = 0;
        context.window.input().key = 0;
    }
    if (context.window.input().key == 27)
    {
        comskip::request_exit(1);
    }
    if (context.window.input().key == 'G')
    {
        context.state.subsample_video  = 0x3f;
        context.window.input().key = 0;
    }
    if (context.state.subsample_video == 0)
    {
        //	Enable for single stepping trough the video
        if (!context.window.is_open() || context.state.review_source_width != context.state.videowidth || context.state.review_source_height != context.state.height)
        {
            if (context.state.width == 0 /*|| (loadingCSV && !showVideo) */)
                context.state.videowidth = context.state.width = 800; // MAXWIDTH;
            if (context.state.height == 0 /*||  (loadingCSV && !showVideo) */)
                context.state.height = 600-barh; // MAXHEIGHT-30;

            if (context.state.height > 600 || context.state.width > 800)
            {
                context.state.oheight = context.state.height / 2;
                context.state.owidth = context.state.width / 2;
                context.state.divider = 2;
            }
            else
            {
                context.state.oheight = context.state.height;
                context.state.owidth = context.state.width;
                context.state.divider = 1;
            }
            context.state.owidth = (context.state.owidth + 31) & -32;
            std::string title;
            comskip::checked_format(title, context.settings.windowtitle.c_str(), context.state.filename);
            context.window.close();
            context.state.ensure_review_graph(context.state.owidth, context.state.oheight + barh);
            context.window.open(context.state.owidth, context.state.oheight + barh, title);
            context.state.review_source_width = context.state.videowidth;
            context.state.review_source_height = context.state.height;

        }

        while (context.window.input().key == 0 && !context.window.input().quit_requested)
            context.window.wait();
        if (context.window.input().key == 27)
        {
            comskip::request_exit(1);
        }
        if (context.window.input().key == 'G')
        {
            context.state.subsample_video  = 0x3f;
        }
        context.window.input().key = 0;
    }

    context.state.recalculate = 0;
}



void Recalc(RecordingContext& context)
{
    BuildBlocks(context, true);
    if (context.settings.commDetectMethod & LOGO)
    {
        PrintLogoFrameGroups(context);
    }
    WeighBlocks(context);

    OutputBlocks(context);
}

bool ReviewResult(RecordingContext& context)
{
    comskip::platform::FilePtr review_file;
    int curframe = 1;
    int lastcurframe = -1;
    int bartop = 0;
    int grf = 2;
    int i,j;
    long prev;
    if (!context.state.framearray) grf = 0;
    context.settings.output_demux = 0;
    context.settings.output_data = 0;
    context.settings.output_srt = 0;
    context.settings.output_smi = 0;
    if (!context.state.mpegfilename.empty()) {
        const auto candidates = comskip::ui::review_media_candidates(context.state.mpegfilename);
        for (std::size_t candidate = 0; candidate < candidates.size(); ++candidate) {
            const auto encoded = candidates[candidate].u8string();
            const std::string filename(encoded.begin(), encoded.end());
            review_file.reset(myfopen(filename.c_str(), "rb"));
            if (!review_file) continue;
            if (candidate != 0) {
                context.state.mpegfilename = filename;
                if (candidate == 1) context.state.demux_pid = 1;
                else context.state.demux_asf = 1;
            }
            break;
        }
    }
    while (true)
    {
        if (context.window.input().quit_requested) comskip::request_exit(0);
        // Indicates whether to force the debug window to refresh even if the current context.state.frame does not change.
        bool forceRefresh = false;

        if (context.window.input().key != 0)
        {
            if (context.window.input().key == 27) if (!context.state.helpflag) comskip::request_exit(0);
            if (context.window.input().key == 112)
            {
                context.state.helpflag = 1;     // F1 Key
                context.state.oldfrm = -1;
            }
            else
            {
                if (context.state.helpflag == 1)
                {
                    context.state.helpflag = 0;
                    context.state.oldfrm = -1;
                }
            }
            if (context.window.input().key == 16)
            {
                context.window.input().shift = 1;
            }
            if (context.window.input().key == 37) curframe -= 1;
            if (context.window.input().key == 39) curframe += 1;
            if (context.window.input().key == 38) curframe -= (int)context.settings.fps;
            if (context.window.input().key == 40) curframe += (int)context.settings.fps;
            if (context.window.input().key == 33) curframe -= (int)(20*context.settings.fps);
            if (context.window.input().key == 133) curframe -= (int)(.5*context.settings.fps);
            if (context.window.input().key == 34) curframe += (int)(20*context.settings.fps);
            if (context.window.input().key == 134) curframe += (int)(.5*context.settings.fps);

            const auto navigate_interval = [&](comskip::ui::IntervalDirection direction) {
                const auto choose = [&](const auto& intervals, int last) {
                    using Entry = std::remove_cvref_t<decltype(intervals[0])>;
                    return comskip::ui::review_interval_boundary(
                        std::span<const Entry>(intervals), last, curframe, direction);
                };
                const auto target = context.state.framearray
                    ? choose(context.state.commercial, context.state.commercial_count)
                    : choose(context.state.reffer, context.state.reffer_count);
                if (target) curframe = static_cast<int>(*target);
            };
            if (context.window.input().key == 78 || (context.window.input().key == 39 && context.window.input().shift))
                navigate_interval(comskip::ui::IntervalDirection::next);
            if (context.window.input().key == 80 || (context.window.input().key == 37 && context.window.input().shift))
                navigate_interval(comskip::ui::IntervalDirection::previous);
            if (context.window.input().key == 'S')
            {
                if (context.state.framearray)
                {
                    curframe = 0;
                }
                else
                {
                    curframe = 	0;

                }
            }
            if (context.window.input().key == 'F')
            {
                if (context.state.framearray)
                {
                    curframe = context.state.frame_count;
                }
                else
                {
                    curframe = 	context.state.frame_count;

                }
            }
            if (context.window.input().key == 'E')  	// End key
            {
                if (context.state.framearray)
                {
                    curframe += 10;
                    i = 0;
                    while (i < context.state.block_count && curframe > context.state.cblock[i].f_end) i++;
                    //					if (i > 0)
                    curframe = context.state.cblock[i].f_end+5;
                    curframe -= 10;
                }
                else
                {
                    i = context.state.reffer_count;
                    while (i >= 0 && curframe < context.state.reffer[i].start_frame) i--;
                    if (i >= 0)
                        context.state.reffer[i].end_frame = curframe;
                    context.state.oldfrm = -1;
                }
            }
            if (context.window.input().key == 'B')  	// begin key
            {
                if (context.state.framearray)
                {
                    curframe -= 10;
                    i = context.state.block_count-1;
                    while (i > 0 && curframe < context.state.cblock[i].f_start) i--;
                    //					if (i > 0)
                    curframe = context.state.cblock[i].f_start-5;
                    curframe += 10;
                }
                else
                {
                    i = 0;
                    while (i <= context.state.reffer_count && curframe > context.state.reffer[i].end_frame) i++;
                    if (i <= context.state.reffer_count)
                        context.state.reffer[i].start_frame = curframe;
                    context.state.oldfrm = -1;
                }
            }
            if (context.window.input().key == 'T')  	// Toggle key
            {
                if (context.state.framearray)
                {
                    i = 0;
                    while (i < context.state.block_count && curframe > context.state.cblock[i].f_end) i++;
                    if (i < context.state.block_count)
                    {
                        if (context.state.cblock[i].score < context.settings.global_threshold)
                            context.state.cblock[i].score = 99.99;
                        else
                            context.state.cblock[i].score = 0.01;
                        context.state.cblock[i].cause |= C_F;
                        context.state.oldfrm = -1;
                        BuildCommercial(context);
                        context.window.input().key = 'W';			// Trick to cause writing of the new commercial list
                    }
                }
            }
            if (context.window.input().key == 68)  	// Delete key
            {
                if (context.state.framearray)
                {
                    i = 0;
                    while (i < context.state.block_count && curframe > context.state.cblock[i].f_end) i++;
                    if (i < context.state.block_count)
                    {
                        context.state.cblock[i].score = 99.99;
                        context.state.cblock[i].cause |= C_F;
                        context.state.oldfrm = -1;
                        BuildCommercial(context);
                    }
                }
                else
                {
                    i = context.state.reffer_count;
                    while (i >= 0 && curframe < context.state.reffer[i].start_frame) i--;
                    if (i >= 0 && context.state.reffer[i].start_frame <= curframe && curframe <= context.state.reffer[i].end_frame )
                    {
                        comskip::detection::erase_interval(context.state.reffer, context.state.reffer_count, i);
                        context.state.oldfrm = -1;
                    }
                }
            }
            if (context.window.input().key == 73)  	// Insert key
            {
                if (context.state.framearray)
                {
                    i = 0;
                    while (i < context.state.block_count && curframe > context.state.cblock[i].f_end) i++;
                    if (i < context.state.block_count)
                    {
                        context.state.cblock[i].score = 0.01;
                        context.state.cblock[i].cause |= C_F;
                        context.state.oldfrm = -1;
                        BuildCommercial(context);
                    }
                }
                else
                {
                    if (comskip::detection::insert_reference(context.state.reffer, context.state.reffer_count,
                                                            curframe, context.state.frame_count))
                    {
                        context.state.oldfrm = -1;
                    }
                }
            }
            if (context.window.input().key == 'W')   // W key
            {
                context.settings.output_default = true;
                OpenOutputFiles(context);
                if (context.state.framearray)
                {
                    prev = -1;
                    for (i = 0; i <= context.state.commercial_count; i++)
                    {
                        OutputCommercialBlock(context, i, prev, context.state.commercial[i].start_frame, context.state.commercial[i].end_frame, (context.state.commercial[i].end_frame < context.state.frame_count-2 ? false : true));
                        prev = context.state.commercial[i].end_frame;
                    }
                    if (context.state.commercial_count < 0 || context.state.commercial[context.state.commercial_count].end_frame < context.state.frame_count-2)
                        OutputCommercialBlock(context, context.state.commercial_count, prev, context.state.frame_count-2, context.state.frame_count-1, true);
                }
                else
                {
                    prev = -1;
                    for (i = 0; i <= context.state.reffer_count; i++)
                    {
                        OutputCommercialBlock(context, i, prev, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, (context.state.reffer[i].end_frame < context.state.frame_count-2 ? false : true));
                        prev = context.state.reffer[i].end_frame;
                    }
                    if (context.state.reffer_count < 0 || context.state.reffer[context.state.reffer_count].end_frame < context.state.frame_count-2)
                        OutputCommercialBlock(context, context.state.reffer_count, prev, context.state.frame_count-2, context.state.frame_count-1, true);
                }
                WriteXmlOutputFiles(context, !context.state.framearray);
                WriteFfmpegSidecarFiles(context, !context.state.framearray);
                WriteFrameScriptFiles(context, !context.state.framearray);
                WritePlayerExportFiles(context, !context.state.framearray);
                context.settings.output_default = false;
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == 'Z')
            {
                if (context.state.zfactor < 256 && context.state.frame_count / context.state.zfactor > context.state.owidth)
                {
//						i = (curframe - zstart) * zfactor * context.state.owidth/ frame_count;
                    context.state.zfactor = context.state.zfactor << 1;
//						zstart = i * frame_count / context.state.owidth / zfactor;
                    context.state.zstart = (curframe + context.state.zstart) / 2;
                    context.state.oldfrm = -1;
                }
            }
            if (context.window.input().key == 'U')
            {
                if (context.state.zfactor > 1)
                {
//						i = (curframe - zstart) * zfactor * context.state.owidth/ frame_count;
                    context.state.zfactor = context.state.zfactor >> 1;
//						zstart = i * frame_count / context.state.owidth / zfactor;
                    context.state.zstart = context.state.zstart - (curframe - context.state.zstart);
                    if (context.state.zstart < 0)
                        context.state.zstart = 0;
                    context.state.oldfrm = -1;

                }
            }
            if (context.window.input().key == 'C')
            {
                RecordCutScene(context, curframe, context.state.frame[curframe].brightness);
            }

            if (context.window.input().key == 'X')
            {
                context.state.show_XDS = !context.state.show_XDS;
                context.state.oldfrm = -1;
            }

            if (context.window.input().key == 'V')
            {
                context.state.show_silence = !context.state.show_silence;
                context.state.oldfrm = -1;
            }

            if (context.window.input().key == 'G')
            {
                grf++;
                if (grf > 2)
                    grf = 0;
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == 113)  				// F2 key
            {
                context.settings.max_volume = (int)(context.settings.max_volume / 1.1);
                Recalc(context);
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == 114)  				// F3 key
            {
                context.settings.non_uniformity = (int)(context.settings.non_uniformity / 1.1);
                Recalc(context);
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == 115)  				// F4 key
            {
                context.settings.max_avg_brightness = (int)(context.settings.max_avg_brightness / 1.1);
                Recalc(context);
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == 116)  				// F5 key
            {
                context.state.timeflag++;
                if (context.state.timeflag > MAXTIMEFLAG)
                    context.state.timeflag = 0;
                context.state.oldfrm = -1;
            }
            if (context.window.input().key == '.')
            {
                context.state.oldfrm = -1;
            }

            if (context.window.input().key == 'J')
            {
                // Handle the user setting the before marker context.state.frame.
                context.state.preMarkerFrame = curframe;
                if (context.state.postMarkerFrame > 0 && context.state.preMarkerFrame > 0)
                {
                    long midpoint = ((long)context.state.postMarkerFrame + (long)context.state.preMarkerFrame) / 2l;
                    curframe = (int)midpoint;
                }

                forceRefresh = true;
            }

            if (context.window.input().key == 'K')
            {
                // Handle the user setting the after marker context.state.frame.
                context.state.postMarkerFrame = curframe;

                if (context.state.postMarkerFrame > 0 && context.state.preMarkerFrame > 0)
                {
                    long midpoint = ((long)context.state.postMarkerFrame + (long)context.state.preMarkerFrame) / 2l;
                    curframe = (int)midpoint;
                }

                forceRefresh = true;
            }

            if (context.window.input().key == 'L')
            {
                // Handle the user clearing the markers.
                context.state.preMarkerFrame = 0;
                context.state.postMarkerFrame = 0;
                forceRefresh = true;
            }

            if (context.window.input().key == 82) return(true);

            context.window.input().key = 0;
        }
        if (context.window.input().mouse_pressed)
        {
            if (context.window.input().mouse_y >= bartop && context.window.input().mouse_y < bartop + 30)
                curframe = context.state.zstart+context.state.frame_count * context.window.input().mouse_x / context.state.owidth / context.state.zfactor + 1;
            context.window.input().mouse_pressed = 0;
        }
        if (curframe < 1) curframe = 1;
        if (context.state.frame_count > 0)
        {
            if (curframe >= context.state.frame_count) curframe = context.state.frame_count-1;
        }
        if (context.state.frame_count > 0 && review_file)
            if (curframe!= lastcurframe)
            {
                DecodeOnePicture(context, review_file.get(), (context.state.framearray ? get_frame_pts(context, curframe) : (double)curframe / context.settings.fps));
                lastcurframe = curframe;
            }
        OutputDebugWindow(context, (review_file ? true : false),curframe, grf, forceRefresh);

        context.window.wait();
    }
    return false;
}


