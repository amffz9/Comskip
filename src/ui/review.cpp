#include "legacy_detection.h"

int oheight = 0;
int owidth = 0;
double divider = 1;
int oldfrm = -1;
int zstart = 0;
int zfactor = 1;
int show_XDS=0;
int show_silence=0;
int preMarkerFrame = 0;
int postMarkerFrame = 0;

void OutputDebugWindow(bool showVideo, int frm, int grf, bool forceRefresh)
{
#if defined(_WIN32) || defined(HAVE_SDL)
    int i,j,x,y,a=0,c=0,r,s=0,g,gc,lb=0,e=0,n=0,bl,xd;
    int v,w;
    int bartop = 0;
    int b,cb;
    int barh = 32;
    char t[1024];
    char x1[80];
    char x2[80];
    char x3[80];
    char x4[80];
    char x5[80];
    const char *tt[40];
    char tbuf[80][80];
    char frametext[80];
    bool	blackframe, bothtrue, haslogo, uniformframe;
    int silence=0;
//	frm++;
    if (!forceRefresh && oldfrm == frm)
        return;
    oldfrm = frm;
    if (output_debugwindow && frame_count )
    {

        if (!vo_init_done)
        {
            if (width == 0 /*|| (loadingCSV && !showVideo) */)
                videowidth = width = 800; // MAXWIDTH;
            if (height == 0 /*||  (loadingCSV && !showVideo) */)
                height = 600-barh; // MAXHEIGHT-30;
            if (edge_step == 0) {
                edge_step = 1;
            }
            if (height > 600 || width > 800)
            {
                oheight = height / 2;
                owidth = width / 2 ;
                owidth = videowidth / 2 ;
                divider = 2;
            }
            else
            if (height < 150 || width < 200)
            {
                divider = 0.25;
                oheight = height / divider;
                owidth = width / divider;
                owidth = videowidth / divider;
            } else
            if (height < 300 || width < 400)
            {
                divider = 0.5;
                oheight = height / divider;
                owidth = width / divider;
                owidth = videowidth / divider;
            } else
            {
                oheight = height;
                owidth = width;
                owidth = videowidth;
                divider = 1;
            }
            oheight = (oheight + 31) & -32;
            owidth = (owidth + 31) & -32;
            sprintf(t, windowtitle, filename);
            vo_init(owidth, oheight+barh,t);
//			vo_init(owidth, oheight+barh,"Comskip");
            vo_init_done++;
        }
//		bartop = oheight;
        if (frm >= frame_count)
            frm = frame_count-1;
        if (frm < 1)
            frm = 1;

        v = frame_count/zfactor;

        if ( frm < zstart + v / 10) zstart = frm - v / 10;
        if ( zstart < 0 ) zstart = 0;


        if ( frm > v + zstart - v / 10) zstart = frm - v + v / 10;

        if (zstart + v > frame_count) zstart = frame_count - v;
//		if ( frm > v + zstart) zstart = frm - v;

        w = ((frm - zstart)* owidth / v);



        if (showVideo && frame_ptr)
        {
            memset(graph, 0, owidth*oheight*3);
            /*
            			for (x = 0; x < border; x++) {
            				for (y = 0; y < oheight; y++) {
            					PIXEL(x,y+barh) = 0;
            					PIXEL(owidth - 1 - x,y+barh) = 0;
            				}
            			}
            */
            for (x = 0+border; x < owidth-border; x++)
            {
//				for (y = 0; y < border; y++) {
//					PIXEL(x,y+barh) = 0;
//					PIXEL(x,oheight - 1 - (y+barh)) = 0;
//				}
                for (y = 0+border; y < oheight-border; y++)
                {
                    if (x*divider < width && y*divider < height)
                        PIXEL(x,y+barh) = frame_ptr[((int)(y*divider))*(width)+(int)(x*divider)] >> (grf?1:0);
//					PIXEL(x,y+barh) = min_br[(y*divider)*width+(x*divider)];		//MAXMIN Logo search

//					PIXEL(x,y+barh) = vert_edges[(y*divider)*width+(x*divider)];	//Edge detect

//					PIXEL(x,y+barh) = (ver_edgecount[(y*divider)*width+(x*divider)]* 4);		// Edge count
                    /*
                    					PIXEL(x,y+barh) =(abs((frame_ptr[(y*divider)*width+(x*divider)] +
                    										      frame_ptr[(y*divider)*width+((x+1)*divider)])/2
                    											  -
                    											  (frame_ptr[(y*divider)*width+((x+2)*divider)]+
                    											  frame_ptr[(y*divider)*width+((x+3)*divider)])/2
                    										) > edge_level_threshold ? 200 : 0);
                    */
                    //					graph[((oheight - y)*owidth+x)*3+0] = frame_ptr[y*owidth+x];
                    //					graph[((oheight - y)*owidth+x)*3+1] = frame_ptr[y*owidth+x];
                    //					graph[((oheight - y)*owidth+x)*3+2] = frame_ptr[y*owidth+x];
                }
            }
            //			memcpy(&graph[owidth*oheight * 0], frame_ptr, owidth*oheight);
            //			memcpy(&graph[owidth*oheight * 1], frame_ptr, owidth*oheight);
            //			memcpy(&graph[owidth*oheight * 2], frame_ptr, owidth*oheight);
            if (framearray && grf && ((commDetectMethod & LOGO) || logoInfoAvailable ))
            {
                if (aggressive_logo_rejection)
                    s = edge_radius/2;				// Cater of mask offset
                else
                    s = 0;
//				w = 0;
//				v = 0;
                if (logoInfoAvailable)  	// Show logo mask
                {
                    if (frame[frm].currentGoodEdge > logo_threshold)
                    {
                        e = (int)(frame[frm].currentGoodEdge * 250);
                        for (y = clogoMinY; y <= clogoMaxY ; y += edge_step)
                        {
                            for (x = clogoMinX; x <= clogoMaxX ; x += edge_step)
                            {
                                if (choriz_edgemask[y * width + x]) r = 255;
                                else r = 0;
                                if (cvert_edgemask[y * width + x]) g = 255;
                                else g = 0;
                                if (r || g) SETPIXEL(((int)((x-s)/divider)),((int)((y-s)/divider))+barh,r,g,0);
                            }
                        }
                    }
                }
                else  					// Show detected logo pixels only while scanning input
                {
                    if (frm+1 == frame_count)
                    {

                        LOGO_X_LOOP
                        {
                            LOGO_Y_LOOP
                            {
                                if (edgemask_filled) {
                                    r = 255 * thoriz_edgemask[(y) * width + (x)];
                                    g = 255 * tvert_edgemask[(y) * width + (x)];
                                } else {
                                    r = 255 * hor_edgecount[(y) * width + (x)] / num_logo_buffers;
                                    g = 255 * ver_edgecount[(y) * width + (x)] / num_logo_buffers;
                                }
                                if (r > 255) r = 255;
                                if (g > 255) g = 255;
                                //if (r > 128 || g >  128)
                                    SETPIXEL(((int)(x/divider)),((int)(y/divider))+barh,r,g,0);

#ifdef xxxxxx
                        for (y = s; y < oheight; y++)
                        {
                            for (x = s ; x < owidth; x++)
                            {
/*
                                if (hor_edgecount[(y*divider) * width + (x*divider)] >= num_logo_buffers*2/3) r = 255;
                                else r = 0;
                                if (ver_edgecount[(y*divider) * width + (x*divider)] >= num_logo_buffers*2/3) g = 255;
                                else g = 0;
 */
                                if (edgemask_filled) {
                                    r = 255 * thoriz_edgemask[((int)((y*divider))) * width + ((int)((x*divider)))] / num_logo_buffers;
                                    g = 255 * tvert_edgemask[((int)((y*divider))) * width + ((int)((x*divider)))] / num_logo_buffers;
                                } else {
                                    r = 255 * hor_edgecount[((int)((y*divider))) * width + ((int)((x*divider)))] / num_logo_buffers;
                                    g = 255 * ver_edgecount[((int)((y*divider))) * width + ((int)((x*divider)))] / num_logo_buffers;
                                }
                                if (r > 255) r = 255;
                                if (g > 255) g = 255;
                                if (r > 128 || g >  128) SETPIXEL(x-s,y-s+barh,r,g,0);
                            }
                        }
#endif
                           }
                        }

                    }

                }

            }
        }
        else
        {
            memset(graph, 0, owidth*oheight*3);

        }


        if (framearray && grf)
        {
            for (y=0; y < oheight; y++)
            {
                SETPIXEL(w,y,100,100,100);
            }
            bl = 0;
            for (x=0 ; x < owidth; x++)  				// debug bar
            {
                a = 0;
                b = 0;
                s = 0;
                c = 0;
                n = 0;
                if (block_count && grf == 2)
                {
                    while (bl < block_count && cblock[bl].f_end < zstart+(int)((double)x * v /owidth))
                        bl++;
#define PLOTS	9

                    PLOT(PLOTS, 0, x, cblock[bl].brightness, 2550, (int)(avg_brightness*punish_threshold), 0, 255, 0); // RED
                    PLOT(PLOTS, 1, x, cblock[bl].volume/100, 100000, (int)(avg_volume*punish_threshold)/100, 255, 0, 0); // Green
                    PLOT(PLOTS, 2, x, cblock[bl].uniform, 3000, (int)(avg_uniform*punish_threshold), 255, 0,0); // RED
                    PLOT(PLOTS, 3, x, (int)(cblock[bl].schange_rate*1000), 1000, (int)(avg_schange*punish_threshold*1000), 255, 0, 0);	// PURPLE
                }
                for (i = zstart+(int)((double)x * v /owidth); i < zstart+(int)((double)(x+1) * v /owidth ); i++)
                {
                    if (i <= frame_count)
                    {
                        b += frame[i].brightness;
                        PLOT(PLOTS, 0, x, frame[i].brightness, 255, max_avg_brightness, 255, 0,0); // RED
                        s += frame[i].volume;
                        PLOT(PLOTS, 1, x, frame[i].volume, 10000, max_volume, (frame[i].audio_channels * 40), 255, 0);			// GREEN
                        e += frame[i].uniform;
                        PLOT(PLOTS, 2, x, frame[i].uniform, 30000, non_uniformity, 0, 255, 255);	// LIGHT BLUE
                        c += (int)(frame[i].currentGoodEdge*100);
                        PLOT(PLOTS, 4, x, (int)(frame[i].currentGoodEdge*100), 100, 0, 255, 255, 0);  // YELLOW
                        PLOT(PLOTS, 4, x, (int)(frame[i].logo_filter*50+50), 100, 0, (frame[i].logo_filter < 0.0 ?255:0) , (frame[i].logo_filter < 0.0 ?0:255), 0);
                        PLOT(PLOTS, 5, x, (int)((frame[i].ar_ratio-0.5) * 100), 250, 0, 0, 0, 255);   // BLUE

                        if (commDetectMethod & CUTSCENE)
                        {
                            PLOT(PLOTS, 3, x, (int)(frame[i].cutscenematch), 100, cutscenedelta, 255, 0, 255);     // PURPLE
                        }
                        else
                        {
                            PLOT(PLOTS, 3, x, (int)(frame[i].schange_percent), 100, schange_cutlevel, 255, 0, 255);	    // PURPLE
                        }
                        a += frame[i].maxY;
                        PLOT(PLOTS, 6, x, frame[i].maxY, height, 0, 0, 128, 128);
                        b += frame[i].minY;
                        PLOT(PLOTS, 6, x, frame[i].minY, height, 0, 0, 128, 128);
                        n++;
                        PLOT(PLOTS, 7, x, frame[i].maxX, width, 0, 0, 0, 255);
                        PLOT(PLOTS, 7, x, frame[i].minX, width, 0, 0, 0, 255);
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
// PLOT(S, I, X, Y, MAX, L, R,G,B)
            }
        }



        if (frame_ptr && framearray)
        {
//			for (x=0; x < owidth; x++) { // Edge counter indicator
//				graph[2* owidth + x] = (x < edge_count /8 ? 255 : 0);
//			}

            x = frame[frm].maxX/divider;
            if (x == 0) x = owidth;
            for (i=frame[frm].minX/divider; i < x; i++)  				// AR lines
            {
                SETPIXEL(i, ((int)((frame[frm].minY/divider)))+barh, 0,0,255);
                SETPIXEL(i, ((int)((frame[frm].maxY/divider)+barh)), 0,0,255);

//				graph[frame[frm].minY* owidth + i] = 255;
//				graph[frame[frm].maxY* owidth + i] = 255;
            }
            for (i=(frame[frm].minY/divider); i < (frame[frm].maxY/divider); i++)  				// AR lines
            {
                SETPIXEL(((int)((frame[frm].minX/divider))), i+barh, 0,0,255);
                SETPIXEL(((int)((frame[frm].maxX/divider))), i+barh, 0,0,255);
            }


        }
        if (framearray /* && commDetectMethod & LOGO */ )
        {
#define SHOWLOGOBOXWHILESCANNING
#ifdef SHOWLOGOBOXWHILESCANNING
            for (x = tlogoMinX/divider; x < tlogoMaxX/divider; x++)  		// Logo box X
            {
                SETPIXEL(x,((int)(tlogoMinY/divider))+barh,255,e,e);
                SETPIXEL(x,((int)(tlogoMaxY/divider))+barh,255,e,e);
            }
            for (y = tlogoMinY/divider; y < tlogoMaxY/divider; y++)  		// Logo box Y
            {
                SETPIXEL(((int)(tlogoMinX/divider)),y+barh,255,e,e);
                SETPIXEL(((int)(tlogoMaxX/divider)),y+barh,255,e,e);
            }
#else
            for (x = clogoMinX/divider; x < clogoMaxX/divider; x++)  		// Logo box X
            {
                SETPIXEL(x,clogoMinY/divider+barh,255,e,e);
                SETPIXEL(x,clogoMaxY/divider+barh,255,e,e);
            }
            for (y = clogoMinY/divider; y < clogoMaxY/divider; y++)  		// Logo box Y
            {
                SETPIXEL(clogoMinX/divider,y+barh,255,e,e);
                SETPIXEL(clogoMaxX/divider,y+barh,255,e,e);
            }
#endif // SHOWLOGOBOXWHILESCANNING
        }

        b = 0;
        for (i = 0; i < block_count; i++)
        {
            if (cblock[i].f_start <= frm && frm <= cblock[i].f_end)
            {
                b = i;
                break;
            }
        }

        /*
        		memset(graph,20,owidth*(oheight+30)*3);
        		for (i=0; i<oheight/2;i++) {
        			graph[(i*(owidth+0))*3] = 255;
        			graph[(i*(owidth+0))*3+1] = 0;
        			graph[(i*(owidth+0))*3+2] = 0;
        		}
        */
//		if (0)	// disable debug bar
        for (x=0 ; x < owidth; x++)  				// debug bar
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
            if (framearray)
            {
                xd = XDS_block_count-1;
                while (xd > 0 && XDS_block[xd].frame > zstart+(int)((double)(x+1) * v /owidth) )
                    xd--;
                if (!(xd > 0 && XDS_block[xd].frame >= zstart+(int)((double)x * v /owidth)))
                    xd = 0;

                for (i = zstart+(int)((double)x * v /owidth); i < zstart+(int)((double)(x+1) * v /owidth ); i++)
                {
                    if (i <= frame_count)
                    {
                        if ((frame[i].isblack & C_b) || (frame[i].isblack & C_r))
                        {
                            blackframe = true;
                            for (j = 0; j < block_count; j++)
                            {
                                if (cblock[j].f_end == i)
                                    bothtrue = true;
                            }
                        }
                        if (frame[i].isblack & C_u)
                        {
                            uniformframe = true;
                        }
                        if (frame[i].volume < max_volume && silence < 1) silence = 1;
                        if ((frame[i].volume < 50 || frame[i].volume < max_silence) && silence < 2) silence = 2;
                        if (frame[i].volume < 9) silence = 3;
                        if (frame[i].volume < 9) silence = 3;
                        if ((frame[i].isblack & C_v)) silence = 3;
                        if (frame[i].volume == 0) silence = 4;
                        if ((frame[i].isblack & C_b) && frame[i].volume < max_volume) bothtrue = true;
                        if ((frame[i].isblack & C_r)) bothtrue = true;
                        if (frm+1 == frame_count)  						// Show details of logo while scanning
                        {
                            if (frame[i].logo_present) haslogo = true;
                        }
                        else
                        {
                            while (lb < logo_block_count && i > logo_block[lb].end)			// Show logo blocks when finished
                                lb++;
                            if (lb < logo_block_count && i >= logo_block[lb].start)
                            {
                                haslogo=true;
                            }
                        }
//					if (frame[i].currentGoodEdge > logo_threshold) haslogo = true;
                        a = (int)((frame[i].ar_ratio - 0.5 - 0.1)*6);		// Position of AR line
                        g += (int)(frame[i].currentGoodEdge * 5);
                        gc++;
                    }
                }
            }
            if (gc > 0)
                g /= gc;
            c = 255;
            if (c == 255)
            {
                for (i = 0; i <= commercial_count; i++)  	// Inside commercial?
                {
                    if (zstart+(int)((double)x * v /owidth ) >= commercial[i].start_frame &&
                            zstart+(int)((double)x * v /owidth ) <= commercial[i].end_frame )
                    {
                        c = 128;
                        break;
                    }
                }
            }

            if (c == 255)  									// not in a commercial but score above threshold
            {
                for (i = 0; i < block_count; i++)
                {
                    if (zstart+(int)((double)x * v /owidth ) >= cblock[i].f_start &&
                            zstart+(int)((double)x * v /owidth ) <= cblock[i].f_end &&
                            cblock[i].score > global_threshold )
                    {
                        c = 220;
                        break;
                    }
                }
            }


            r = 255;
            for (i = 0; i <= reffer_count; i++)  		// Inside reference?
            {
                if (zstart+(int)((double)x * v /owidth ) >= reffer[i].start_frame &&
                        zstart+(int)((double)x * v /owidth ) <= reffer[i].end_frame )
                {
                    r = 0;
                    break;
                }
            }

            a = bartop + 14 - a;
            for (y = bartop+5; y < bartop+15 ; y++)			// Commercial / AR bar
                if (y == a)
                {
                    SETPIXEL(x,y,0,0,255);
                }
                else
                {
                    SETPIXEL(x,y,c,c,c);
//					PIXEL(x,y) = c;
                }
            g = 5; // Disable goodEdge graph
            for (i = 0; i < block_count; i++)
            {
                if (zstart+(int)((double)x * v /owidth ) >= cblock[i].f_start &&
                        zstart+(int)((double)x * v /owidth ) <= cblock[i].f_end &&
                        cblock[i].correlation > 0 )  					// if inside a correlated cblock
                {
                    g=2;
                    break;
                }
            }
            for (y = bartop + 15; y < bartop+20 ; y++)  		// Logo bar
            {

                if (haslogo) PIXEL(x,y) = ((y - (bartop + 15) == g)?255:((commDetectMethod & LOGO)? 0 : 128));
                else PIXEL(x,y) = ((y - (bartop + 15) == g)?0:255);
//				if (y - (bartop + 15) == g) graph[y * owidth + x] = 128;
            }

            cb = 255;
            if (block_count && cblock[b].f_start <= zstart+(int)((double)x * v /owidth ) && zstart+(int)((double)x * v /owidth ) <= cblock[b].f_end)
                cb = 0;

            if (bothtrue)
                c = 0;
            else
                c = 255;
            for (y = bartop + 20; y < bartop+25 ; y++)      // Blackframe bar
            {
                if (blackframe)
                {
                    SETPIXEL(x,y,c,0,0);
                }
                else if (uniformframe)
                {
                    SETPIXEL(x,y,0,0,c);
                }
                else
                {
                    SETPIXEL(x,y,255,cb,255);
                }
            }
            c = 255;

            for (y = bartop + 25; y < bartop+30 ; y++)  	// Silence bar
            {
                if (silence == 1)
                {
                    SETPIXEL(x,y,0,c,0);
                }
                else if (silence == 2)
                {
                    SETPIXEL(x,y,0,0,c);
                }
                else if (silence == 3)
                {
                    SETPIXEL(x,y,c,0,0);
                }
                else if (silence == 4)
                {
                    SETPIXEL(x,y,c,c,0);
                }
                else
                    PIXEL(x,y) = 255;
            }
            if (w < owidth)									// Progress indicator
                for (y = bartop; y < bartop+30 ; y++) SETPIXEL(w,y,255,0,0);

            if (preMarkerFrame > 0)
            {
                int showMkrX = ((preMarkerFrame - zstart)* owidth / v);
                for (y = bartop; y < bartop+30 ; y++) SETPIXEL(showMkrX,y,0,255,0);
            }

            if (postMarkerFrame > 0)
            {
                int comMkrX = ((postMarkerFrame - zstart)* owidth / v);
                for (y = bartop; y < bartop+30 ; y++) SETPIXEL(comMkrX,y,0,0,255);
            }

            for (y = bartop; y < bartop+(loadingTXT?20:5) ; y++)
            {
                // Reference bar
                if (xd)
                {
                    SETPIXEL(x,y,128,128,128);
                }
                else if (reffer_count >= 0) PIXEL(x,y) = r;
            }
        }
        vo_draw(graph);

        //		sprintf(t, "%8i %8i %1s %1s", frm, framenum_infer, (frame[frm].isblack?"B":" "), (frame[frm].volume<max_volume?"S":" "));
        b = 0;
        for (i = 0; i < block_count; i++)
        {
            if (cblock[i].f_start <= frm && frm <= cblock[i].f_end)
            {
                b = i;
                break;
            }
        }
        if (timeflag == 2 && framearray)
        {
            sprintf(frametext, "%8.2f", F2T(frm));
        }
        else
        if (timeflag == 1 && framearray)
        {
            sprintf(frametext, "%s", dblSecondsToStrMinutes(F2T(frm)));
        }
        else
        {

            sprintf(frametext, "%8i", frm);
        }

        if (recalculate)
        {
            sprintf(t, "max_volume=%d, non_uniformity=%d, max_avg_brightness=%d", max_volume, non_uniformity, max_avg_brightness);
        }
        else
        {
            if (framearray)
            {
                if (b < block_count)
                    sprintf(t, "%s B=%i%1s V=%i%1s U=%i%1s AR=%4.2f  Block #%i Length=%6.2fs Score=%6.2f Logo=%6.2f %s                      ", frametext, frame[frm].brightness, (frame[frm].isblack & C_b?"B":" "), frame[frm].volume, (frame[frm].volume<max_volume?"S":" "),frame[frm].uniform, (frame[frm].uniform<non_uniformity?"U":" "), frame[frm].ar_ratio, b, cblock[b].length, cblock[b].score, cblock[b].logo, CauseString(cblock[b].cause)
                           );
                else
                    sprintf(t, "%s B=%i%1s V=%i%1s U=%i%1s AR=%4.2f                                                                          ", frametext, frame[frm].brightness, (frame[frm].isblack & C_b?"B":" "), frame[frm].volume, (frame[frm].volume<max_volume?"S":" "),frame[frm].uniform, (frame[frm].uniform<non_uniformity?"U":" "), frame[frm].ar_ratio);
            }
            else
                sprintf(t, "%s", frametext);
        }
        if (soft_seeking)
        {
            tt[0] = t;
            tt[1] = "WARNING: Seeking inaccurate, do not use for cutpoint review!";
            tt[2] = 0;
            ShowHelp(tt);
        }
        else if (helpflag)
            ShowHelp(helptext);
        else if (show_XDS && XDS_block_count)
        {
            tt[0] = t;
            i = (XDS_block_count > 0 ? XDS_block_count-1 : 0);
            while (i > 0 && XDS_block[i].frame > frm)
                i--;
            sprintf(x1,"Program Name    : %s", XDS_block[i].name);
            tt[1] = x1;
            sprintf(x2,"Program V-Chip  : %4x", XDS_block[i].v_chip);
            tt[2] = x2;
            sprintf(x3,"Program duration: %2d:%02d", (XDS_block[i].duration & 0x3f00)/256, (XDS_block[i].duration & 0x3f) % 256);
            tt[3] = x3;
            sprintf(x4,"Program position: %2d:%02d", (XDS_block[i].position & 0x3f00)/256, (XDS_block[i].position & 0x3f) % 256);
            tt[4] = x4;
            sprintf(x5,"Composite Packet: %2d:%02d, %2d/%2d, ", (XDS_block[i].composite1 & 0x3f00)/256, (XDS_block[i].composite1 & 0x1f) % 256, (XDS_block[i].composite2 & 0x1f00)/256, (XDS_block[i].composite2 & 0x0f) % 256);
            tt[5] = x5;
            tt[6] = 0;
            ShowHelp(tt);
        }
        else if (show_silence)
        {
            for (i=0; i<25; i++)
            {
                tt[i] = tbuf[i];
                sprintf(tbuf[i],"volume[%i] = %i", i, silenceHistogram[i]);
            }
            tt[i] = 0;
            ShowHelp(tt);
        }
        else
            ShowDetails(t);
    }
    if (key == 0x20)
    {
        subsample_video  = 0;
        key = 0;
    }
    if (key == 27)
    {
        exit(1);
    }
    if (key == 'G')
    {
        subsample_video  = 0x3f;
        key = 0;
    }
    if (subsample_video == 0)
    {
        //	Enable for single stepping trough the video
        if (!vo_init_done)
        {
            if (width == 0 /*|| (loadingCSV && !showVideo) */)
                videowidth = width = 800; // MAXWIDTH;
            if (height == 0 /*||  (loadingCSV && !showVideo) */)
                height = 600-barh; // MAXHEIGHT-30;

            if (height > 600 || width > 800)
            {
                oheight = height / 2;
                owidth = width / 2;
                divider = 2;
            }
            else
            {
                oheight = height;
                owidth = width;
                divider = 1;
            }
            owidth = (owidth + 31) & -32;
            sprintf(t, windowtitle, filename);
            vo_init(owidth, oheight+barh,t);
//			vo_init(owidth, oheight+barh,"Comskip");
            vo_init_done++;
        }

        while(key == 0)
            vo_draw(graph);
        if (key == 27)
        {
            exit(1);
        }
        if (key == 'G')
        {
            subsample_video  = 0x3f;
        }
        key = 0;
    }

    recalculate = 0;
#endif
}

int shift = 0;

void Recalc()
{
    BuildBlocks(true);
    if (commDetectMethod & LOGO)
    {
        PrintLogoFrameGroups();
    }
    WeighBlocks();

    OutputBlocks();
}

bool ReviewResult()
{
    FILE *review_file = NULL;
    int curframe = 1;
    int lastcurframe = -1;
    int bartop = 0;
    int grf = 2;
    int i,j;
    long prev;
    char tsfilename[MAX_PATH];
    if (!framearray) grf = 0;
    output_demux = 0;
    output_data = 0;
    output_srt = 0;
    output_smi = 0;
    if (!review_file && mpegfilename[0])
        review_file = myfopen(mpegfilename, "rb");
    if (review_file == 0 )
    {
        strcpy(tsfilename, mpegfilename);
        i = strlen(tsfilename);
        while (i > 0 && tsfilename[i-1] != '.') i--;
        tsfilename[i] = 't';
        tsfilename[i+1] = 's';
        tsfilename[i+2] = 0;
        review_file = myfopen(tsfilename, "rb");
        if (review_file)
        {
            demux_pid = 1;
            strcpy(mpegfilename, tsfilename);
        }
    }
    if (review_file == 0 )
    {
        strcpy(tsfilename, mpegfilename);
        i = strlen(tsfilename);
        while (i > 0 && tsfilename[i-1] != '.') i--;
        strcpy(&tsfilename[i], "dvr-ms");
        review_file = myfopen(tsfilename, "rb");
        if (review_file)
        {
            demux_asf = 1;
            strcpy(mpegfilename, tsfilename);
        }
    }
    while(true)
    {
        // Indicates whether to force the debug window to refresh even if the current frame does not change.
        bool forceRefresh = false;

        if (key != 0)
        {
            if (key == 27) if (!helpflag) exit(0);
            if (key == 112)
            {
                helpflag = 1;     // F1 Key
                oldfrm = -1;
            }
            else
            {
                if (helpflag == 1)
                {
                    helpflag = 0;
                    oldfrm = -1;
                }
            }
            if (key == 16)
            {
                shift = 1;
            }
            if (key == 37) curframe -= 1;
            if (key == 39) curframe += 1;
            if (key == 38) curframe -= (int)fps;
            if (key == 40) curframe += (int)fps;
            if (key == 33) curframe -= (int)(20*fps);
            if (key == 133) curframe -= (int)(.5*fps);
            if (key == 34) curframe += (int)(20*fps);
            if (key == 134) curframe += (int)(.5*fps);

            if (key == 78 || (key == 39 && shift))   // Next key
            {
                curframe += 5;
                if (framearray)
                {
                    i = 0;
                    while (i <= commercial_count && curframe > commercial[i].end_frame) i++;
                    //					if (i > 0)
                    curframe = commercial[i].end_frame+5;
//						while (curframe < frame_count && frame[curframe].isblack) curframe++;
//						while (curframe < frame_count && !frame[curframe].isblack) curframe++;
                    //					while (curframe < frame_count && frame[curframe].isblack) curframe++;
                }
                else
                {
                    i = 0;
                    while (i <= reffer_count && curframe > reffer[i].end_frame) i++;
                    //					if (i > 0)
                    curframe = reffer[i].end_frame+5;
                }
                curframe -= 5;
            }
            if (key == 80 || (key == 37 && shift))  	// Prev key
            {
                curframe -= 5;
                if (framearray)
                {
                    i = commercial_count;
                    while (i >= 0 && curframe < commercial[i].start_frame) i--;
                    //					if (i > 0)
                    curframe = commercial[i].start_frame-5;
                    //					while (curframe > 1 && frame[curframe].isblack) curframe--;
                    //					while (curframe > 1 && !frame[curframe].isblack) curframe--;
                    //					while (curframe > 1 && frame[curframe].isblack) curframe--;
                }
                else
                {
                    i = reffer_count;
                    while (i >= 0 && curframe < reffer[i].start_frame) i--;
                    //					if (i > 0)
                    curframe = reffer[i].start_frame-5;
                }
                curframe += 5;
            }
            if (key == 'S')
            {
                if (framearray)
                {
                    curframe = 0;
                }
                else
                {
                    curframe = 	0;

                }
            }
            if (key == 'F')
            {
                if (framearray)
                {
                    curframe = frame_count;
                }
                else
                {
                    curframe = 	frame_count;

                }
            }
            if (key == 'E')  	// End key
            {
                if (framearray)
                {
                    curframe += 10;
                    i = 0;
                    while (i < block_count && curframe > cblock[i].f_end) i++;
                    //					if (i > 0)
                    curframe = cblock[i].f_end+5;
                    curframe -= 10;
                }
                else
                {
                    i = reffer_count;
                    while (i >= 0 && curframe < reffer[i].start_frame) i--;
                    if (i >= 0)
                        reffer[i].end_frame = curframe;
                    oldfrm = -1;
                }
            }
            if (key == 'B')  	// begin key
            {
                if (framearray)
                {
                    curframe -= 10;
                    i = block_count-1;
                    while (i > 0 && curframe < cblock[i].f_start) i--;
                    //					if (i > 0)
                    curframe = cblock[i].f_start-5;
                    curframe += 10;
                }
                else
                {
                    i = 0;
                    while (i <= reffer_count && curframe > reffer[i].end_frame) i++;
                    if (i <= reffer_count)
                        reffer[i].start_frame = curframe;
                    oldfrm = -1;
                }
            }
            if (key == 'T')  	// Toggle key
            {
                if (framearray)
                {
                    i = 0;
                    while (i < block_count && curframe > cblock[i].f_end) i++;
                    if (i < block_count)
                    {
                        if (cblock[i].score < global_threshold)
                            cblock[i].score = 99.99;
                        else
                            cblock[i].score = 0.01;
                        cblock[i].cause |= C_F;
                        oldfrm = -1;
                        BuildCommercial();
                        key = 'W';			// Trick to cause writing of the new commercial list
                    }
                }
            }
            if (key == 68)  	// Delete key
            {
                if (framearray)
                {
                    i = 0;
                    while (i < block_count && curframe > cblock[i].f_end) i++;
                    if (i < block_count)
                    {
                        cblock[i].score = 99.99;
                        cblock[i].cause |= C_F;
                        oldfrm = -1;
                        BuildCommercial();
                    }
                }
                else
                {
                    i = reffer_count;
                    while (i >= 0 && curframe < reffer[i].start_frame) i--;
                    if (i >= 0 && reffer[i].start_frame <= curframe && curframe <= reffer[i].end_frame )
                    {
                        while (i < reffer_count)
                        {
                            reffer[i] = reffer[i+1];
                            i++;
                        }
                        reffer_count--;
                        oldfrm = -1;
                    }
                }
            }
            if (key == 73)  	// Insert key
            {
                if (framearray)
                {
                    i = 0;
                    while (i < block_count && curframe > cblock[i].f_end) i++;
                    if (i < block_count)
                    {
                        cblock[i].score = 0.01;
                        cblock[i].cause |= C_F;
                        oldfrm = -1;
                        BuildCommercial();
                    }
                }
                else
                {
                    i = reffer_count;
                    while (i >= 0 && curframe < reffer[i].start_frame) i--;
                    if (i == -1 || curframe > reffer[i].end_frame )   //Insert BEFORE i
                    {
                        j = reffer_count;
                        while (j > i)
                        {
                            reffer[j+1] = reffer[j];
                            j--;
                        }
                        reffer[i+1].start_frame = max(curframe-1000,1);
                        reffer[i+1].end_frame = min(curframe+1000,frame_count);
                        reffer_count++;
                        oldfrm = -1;
                    }
                }
            }
            if (key == 'W')   // W key
            {
                output_default = true;
                OpenOutputFiles();
                if (framearray)
                {
                    prev = -1;
                    for (i = 0; i <= commercial_count; i++)
                    {
                        OutputCommercialBlock(i, prev, commercial[i].start_frame, commercial[i].end_frame, (commercial[i].end_frame < frame_count-2 ? false : true));
                        prev = commercial[i].end_frame;
                    }
                    if (commercial[commercial_count].end_frame < frame_count-2)
                        OutputCommercialBlock(commercial_count, prev, frame_count-2, frame_count-1, true);
                }
                else
                {
                    prev = -1;
                    for (i = 0; i <= reffer_count; i++)
                    {
                        OutputCommercialBlock(i, prev, reffer[i].start_frame, reffer[i].end_frame, (reffer[i].end_frame < frame_count-2 ? false : true));
                        prev = reffer[i].end_frame;
                    }
                    if (reffer[reffer_count].end_frame < frame_count-2)
                        OutputCommercialBlock(reffer_count, prev, frame_count-2, frame_count-1, true);
                }
                output_default = false;
                oldfrm = -1;
            }
            if (key == 'Z')
            {
                if (zfactor < 256 && frame_count / zfactor > owidth)
                {
//						i = (curframe - zstart) * zfactor * owidth/ frame_count;
                    zfactor = zfactor << 1;
//						zstart = i * frame_count / owidth / zfactor;
                    zstart = (curframe + zstart) / 2;
                    oldfrm = -1;
                }
            }
            if (key == 'U')
            {
                if (zfactor > 1)
                {
//						i = (curframe - zstart) * zfactor * owidth/ frame_count;
                    zfactor = zfactor >> 1;
//						zstart = i * frame_count / owidth / zfactor;
                    zstart = zstart - (curframe - zstart);
                    if (zstart < 0)
                        zstart = 0;
                    oldfrm = -1;

                }
            }
            if (key == 'C')
            {
                RecordCutScene(curframe, frame[curframe].brightness);
            }

            if (key == 'X')
            {
                show_XDS = !show_XDS;
                oldfrm = -1;
            }

            if (key == 'V')
            {
                show_silence = !show_silence;
                oldfrm = -1;
            }

            if (key == 'G')
            {
                grf++;
                if (grf > 2)
                    grf = 0;
                oldfrm = -1;
            }
            if (key == 113)  				// F2 key
            {
                max_volume = (int)(max_volume / 1.1);
                Recalc();
                oldfrm = -1;
            }
            if (key == 114)  				// F3 key
            {
                non_uniformity = (int)(non_uniformity / 1.1);
                Recalc();
                oldfrm = -1;
            }
            if (key == 115)  				// F4 key
            {
                max_avg_brightness = (int)(max_avg_brightness / 1.1);
                Recalc();
                oldfrm = -1;
            }
            if (key == 116)  				// F5 key
            {
                timeflag++;
                if (timeflag > MAXTIMEFLAG)
                    timeflag = 0;
                oldfrm = -1;
            }
            if (key == '.')
            {
                oldfrm = -1;
            }

            if (key == 'J')
            {
                // Handle the user setting the before marker frame.
                preMarkerFrame = curframe;
                if (postMarkerFrame > 0 && preMarkerFrame > 0)
                {
                    long midpoint = ((long)postMarkerFrame + (long)preMarkerFrame) / 2l;
                    curframe = (int)midpoint;
                }

                forceRefresh = true;
            }

            if (key == 'K')
            {
                // Handle the user setting the after marker frame.
                postMarkerFrame = curframe;

                if (postMarkerFrame > 0 && preMarkerFrame > 0)
                {
                    long midpoint = ((long)postMarkerFrame + (long)preMarkerFrame) / 2l;
                    curframe = (int)midpoint;
                }

                forceRefresh = true;
            }

            if (key == 'L')
            {
                // Handle the user clearing the markers.
                preMarkerFrame = 0;
                postMarkerFrame = 0;
                forceRefresh = true;
            }

            if (key == 82) return(true);
            if (key != 16) shift = 0;
            key = 0;
        }
        if (lMouseDown)
        {
            if (yPos >= bartop && yPos < bartop + 30)
                curframe = zstart+frame_count * xPos / owidth / zfactor + 1;
            lMouseDown = 0;
        }
        if (curframe < 1) curframe = 1;
        if (frame_count > 0)
        {
            if (curframe >= frame_count) curframe = frame_count-1;
        }
        if (frame_count > 0 && review_file)
            if (curframe!= lastcurframe)
            {
                DecodeOnePicture(review_file, (framearray ? F2T(curframe) : (double)curframe / fps));
                lastcurframe = curframe;
            }
        OutputDebugWindow((review_file ? true : false),curframe, grf, forceRefresh);
#if defined(_WIN32) || defined(HAVE_SDL)
        vo_wait();
#endif
    }
    return false;
}


