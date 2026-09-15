#include "checked_format.h"
#include "legacy_detection.h"

char TempXmlFilename[300];

char *EscapeXmlFilename(char *f)
{
    char *o = TempXmlFilename;
    while (*f) {
        if (*f == '&') {
            *o++ = '&';
            *o++ = 'a';
            *o++ = 'm';
            *o++ = 'p';
            *o++ = ';';
            f++;
        } else
        if (*f == '<') {
            *o++ = '&';
            *o++ = 'l';
            *o++ = 't';
            *o++ = ';';
            f++;
        } else
        if (*f == '>') {
            *o++ = '&';
            *o++ = 'g';
            *o++ = 't';
            *o++ = ';';
            f++;
        } else
        if (*f == '%') {
            *o++ = '&';
            *o++ = '#';
            *o++ = '3';
            *o++ = '7';
            *o++ = ';';
            f++;
        } else
            *o++ = *f++;
    }
    *o++ = 0;
    return (TempXmlFilename);
}

void OpenOutputFiles()
{
    char	tempstr[MAX_PATH];
    char	cwd[MAX_PATH];

    if (output_default)
    {
        out_file = myfopen(out_filename, "w");
        if (!out_file)
        {
            sleep_for_ms(50L);
            out_file = myfopen(out_filename, "w");
            if (!out_file)
            {
                Debug(0, "ERROR writing to %s\n", out_filename);
                exit(103);
            }
        }
        fprintf(out_file, "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",F2F(frame_count-1), (int)(fps*100));
        fclose(out_file);
    }

    if (output_chapters)
    {
        comskip::checked_format(filename, "%s.chap", outbasename);
        chapters_file = myfopen(filename, "w");
        if (!chapters_file)
        {
            sleep_for_ms(50L);
            out_file = myfopen((const char*)chapters_file, "w");
            if (!chapters_file)
            {
                Debug(0, "ERROR writing to %s\n", filename);
                exit(103);
            }
        }
        fprintf(chapters_file, "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",frame_count-1, (int)(fps*100));
    }

    if (output_zoomplayer_cutlist)
    {
        comskip::checked_format(filename, "%s.cut", outbasename);
        zoomplayer_cutlist_file = myfopen(filename, "w");
        if (!zoomplayer_cutlist_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_zoomplayer_cutlist = true;
//			fclose(zoomplayer_cutlist_file);
        }
    }
    if (output_plist_cutlist)
    {
        comskip::checked_format(filename, "%s.plist", outbasename);
        plist_cutlist_file = myfopen(filename, "w");
        if (!plist_cutlist_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_plist_cutlist = true;
            fprintf(plist_cutlist_file, "<array>\n");
//			fclose(plist_cutlist_file);
        }
    }

    if (output_incommercial)
    {
        comskip::checked_format(filename, "%s.incommercial", workbasename);
        incommercial_file = myfopen(filename, "w");
        if (!incommercial_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        fprintf(incommercial_file, "0\n");
        fclose(incommercial_file);
    }




    if (output_zoomplayer_chapter)
    {
        comskip::checked_format(filename, "%s.chp", outbasename);
        zoomplayer_chapter_file = myfopen(filename, "w");
        if (!zoomplayer_chapter_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_zoomplayer_chapter = true;
//			fclose(zoomplayer_chapter_file);
        }
    }

    if (output_scf)
    {
        comskip::checked_format(filename, "%s.scf", outbasename);
        scf_file = myfopen(filename, "w");
        if (!scf_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_scf = true;
        }
    }

    if (output_edl)
    {
        comskip::checked_format(filename, "%s.edl", outbasename);
        edl_file = myfopen(filename, "wb");
        if (!edl_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_edl = true;
        }
    }

    if (output_ffmeta)
    {
        comskip::checked_format(filename, "%s.ffmeta", outbasename);
        ffmeta_file = myfopen(filename, "wb");
        if (!ffmeta_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_ffmeta = true;
        }
    }

    if (output_ffsplit)
    {
        comskip::checked_format(filename, "%s.ffsplit", outbasename);
        ffsplit_file = myfopen(filename, "wb");
        if (!ffsplit_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_ffsplit = true;
        }
    }
/*
    if (output_live)
    {
        comskip::checked_format(filename, "%s.live", outbasename);
        live_file = myfopen(filename, "wb");
        if (!live_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_live = true;
        }
    }
*/
    if (output_ipodchap)
    {
        comskip::checked_format(filename, "%s.chap", outbasename);
        ipodchap_file = myfopen(filename, "w");
        if (!ipodchap_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_ipodchap = true;
        }
        fprintf(ipodchap_file,"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
    }

    if (output_edlp)
    {
        comskip::checked_format(filename, "%s.edlp", outbasename);
        edlp_file = myfopen(filename, "w");
        if (!edlp_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_edlp = true;
        }
    }


    if (output_bsplayer)
    {
        comskip::checked_format(filename, "%s.bcf", outbasename);
        bcf_file = myfopen(filename, "w");
        if (!bcf_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_bsplayer = true;
        }
    }

    if (output_edlx)
    {
        comskip::checked_format(filename, "%s.edlx", outbasename);
        edlx_file = myfopen(filename, "w");
        if (!edlx_file)
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
        else
        {
            output_edlx = true;
            fprintf(edlx_file, "<regionlist units=\"bytes\" mode=\"exclude\"> \n");
        }
    }


    if (output_videoredo && !output_videoredo3)
    {
//<Version>2
//<Filename>G:\comskip79_46\mpg\MXC_20060518_00000030.mpg
//<Cut>4255584667:5666994667
//<Cut>8590582000:11001991000
//<SceneMarker 0>797115333
//<SceneMarker 1>1083729555
//<SceneMarker 2>4254502333
//<SceneMarker 3>4708947222

        comskip::checked_format(filename, "%s.VPrj", outbasename);
        videoredo_file = myfopen(filename, "w");
        if (videoredo_file)
        {
            if (mpegfilename[1] == ':' || mpegfilename[0] == PATH_SEPARATOR)
            {
                fprintf(videoredo_file, "<Version>2\n<Filename>%s\n", mpegfilename);
            }
            else
            {
                _getcwd(cwd, 256);
                fprintf(videoredo_file, "<Version>2\n<Filename>%s%c%s\n", cwd, PATH_SEPARATOR, mpegfilename);
            }
            if (is_h264)
            {
                fprintf(videoredo_file, "<MPEG Stream Type>4\n");
            }

//			fclose(videoredo_file);
            output_videoredo = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }
    if (output_videoredo3)
    {
        /*
        	<VideoReDoProject Version="3">
           <Filename>D:\My TiVo Recordings\MultipleAudioSample-CDN-96603-234.wtv</Filename>
           <CutList>
              <cut Sequence="1" CutStart="00:00:00;00" CutEnd="00:00:03;09" Elapsed="00:00:00;00">
                 <CutTimeStart>0</CutTimeStart>
                 <CutTimeEnd>33600111</CutTimeEnd>
                 <CutByteStart>0</CutByteStart>
                 <CutByteEnd>2931356</CutByteEnd>
              </cut>
              <cut Sequence="2" CutStart="00:00:05;10" CutEnd="00:00:20;16" Elapsed="00:00:02;01">
                 <CutTimeStart>54000113</CutTimeStart>
                 <CutTimeEnd>206400112</CutTimeEnd>
                 <CutByteStart>4652532</CutByteStart>
                 <CutByteEnd>17301504</CutByteEnd>
              </cut>
           </CutList>
        </VideoReDoProject>VideoReDo

        */

        comskip::checked_format(filename, "%s.VPrj", outbasename);
        videoredo3_file = myfopen(filename, "w");
        if (videoredo3_file)
        {
            if (mpegfilename[1] == ':' || mpegfilename[0] == PATH_SEPARATOR)
            {
                fprintf(videoredo3_file, "<VideoReDoProject Version=\"3\">\n<Filename>%s</Filename><CutList>\n", EscapeXmlFilename(mpegfilename));
            }
            else
            {
                _getcwd(cwd, 256);
                fprintf(videoredo3_file, "<VideoReDoProject Version=\"3\">\n<Filename>%s%c%s</Filename><CutList>\n", cwd, PATH_SEPARATOR, EscapeXmlFilename(mpegfilename));
            }
//              if (is_h264) {
            //                 fprintf(videoredo3_file, "<MPEG Stream Type>4\n");
            //          }

//			fclose(videoredo3_file);
            output_videoredo3 = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_btv)
    {
        comskip::checked_format(filename, "%s.chapters.xml", mpegfilename);
        btv_file = myfopen(filename, "w");
        if (btv_file)
        {
            fprintf(btv_file, "<cutlist>\n");
//			fclose(btv_file);
            output_btv = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_cuttermaran)
    {
        comskip::checked_format(filename, "%s.cpf", outbasename);
        cuttermaran_file = myfopen(filename, "w");
        if (cuttermaran_file)
        {
            if (mpegfilename[1] == ':' || mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, inbasename);
            }
            fprintf(cuttermaran_file, "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
            fprintf(cuttermaran_file, "<StateData xmlns=\"http://cuttermaran.kickme.to/StateData.xsd\">\n");
            fprintf(cuttermaran_file, "<usedVideoFiles FileID=\"0\" FileName=\"%s.M2V\" />\n",inbasename);
            fprintf(cuttermaran_file, "<usedAudioFiles FileID=\"1\" FileName=\"%s.mp2\" StartDelay=\"0\" />\n",inbasename);
//			fclose(cuttermaran_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_vcf)
    {
        comskip::checked_format(filename, "%s.vcf", outbasename);
        vcf_file = myfopen(filename, "w");
        if (vcf_file)
        {
            if (mpegfilename[1] == ':' || mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, inbasename);
            }
            fprintf(vcf_file, "VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
//			fclose(vcf_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_vdr)
    {
        comskip::checked_format(filename, "%s.vdr", outbasename);
        vdr_file = myfopen(filename, "w");
        if (vdr_file)
        {
            if (mpegfilename[1] == ':' || mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, inbasename);
            }
//			fprintf(vdr_file, "VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
//			fclose(vdr_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_projectx)
    {
        comskip::checked_format(filename, "%s.Xcl", mpegfilename);
        projectx_file = myfopen(filename, "w");
        if (projectx_file)
        {
            fprintf(projectx_file, "CollectionPanel.CutMode=2\n");
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_avisynth)
    {
        comskip::checked_format(filename, "%s.avs", mpegfilename);
        avisynth_file = myfopen(filename, "w");
        if (avisynth_file)
        {
            if (avisynth_options[0] == 0)
                fprintf(avisynth_file, "LoadPlugin(\"MPEG2Dec3.dll\") \nMPEG2Source(\"%s\")\n", mpegfilename);
            else
                fprintf(avisynth_file, avisynth_options, mpegfilename);

        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_womble)
    {
        comskip::checked_format(filename, "%s.wme", outbasename);
        womble_file = myfopen(filename, "w");
        if (womble_file)
        {
//			fclose(womble_file);
            output_womble = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_mls)
    {
        comskip::checked_format(filename, "%s.mls", outbasename);
        mls_file = myfopen(filename, "w");
        if (mls_file)
        {
//			fclose(mls_file);
            output_mls = true;
//[BookmarkList]
//PathName= C:\VidTst\Will - Grace - Secrets - Lays.mpg
//VideoStreamID= 224
//Format= frame
//Count= 19

        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_mpgtx)
    {
        comskip::checked_format(filename, "%s_mpgtx.bat", outbasename);
        mpgtx_file = myfopen(filename, "w");
        if (mpgtx_file)
        {
//			fclose(mpgtx_file);
            output_mpgtx = true;
            fprintf(mpgtx_file, "mpgtx.exe -j -f -o \"%s%s\" \"%s\" ", mpegfilename, ".clean", mpegfilename);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_dvrcut)
    {
        comskip::checked_format(filename, "%s_dvrcut.bat", outbasename);
        dvrcut_file = myfopen(filename, "w");
        if (dvrcut_file)
        {
//			fclose(dvrcut_file);
            if (dvrcut_options[0] == 0)
                fprintf(dvrcut_file, "dvrcut \"%%1\" \"%%2\" ");
            else
                fprintf(dvrcut_file, dvrcut_options, inbasename, inbasename, inbasename  );
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_dvrmstb)
    {
        comskip::checked_format(filename, "%s.xml", outbasename);
        dvrmstb_file = myfopen(filename, "w");
        if (dvrmstb_file)
        {
//			fclose(dvrmstb_file);
            fprintf(dvrmstb_file, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<root>\n");
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }

    if (output_mpeg2schnitt)
    {
        comskip::checked_format(filename, "%s_mpeg2schnitt.bat", inbasename);
        mpeg2schnitt_file = myfopen(filename, "w");
        if (mpeg2schnitt_file)
        {
//			fclose(mpeg2schnitt_file);
            output_mpgtx = true;
// Mpeg2Schnitt.exe %1.m2v /R29.97 /o250 /i550 /o3210 /i4000 /S /E /Z %2.m2v
            if (mpeg2schnitt_options[0] == 0)
                fprintf(mpeg2schnitt_file, "mpeg2schnitt.exe /S /E /R%5.2f  /Z \"%s\" \"%s\" ", fps, "%2", "%1");
            else
                fprintf(mpeg2schnitt_file, "%s ", mpeg2schnitt_options);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
            exit(6);
        }
    }
		if (output_mkvtoolnix>0)
	{
		/*
		<?xml version="1.0" encoding="ISO-8859-1"?>
		<Chapters>
			<EditionEntry>
				<ChapterAtom>
					<ChapterDisplay>
						<ChapterString>Comercial</ChapterString>
					</ChapterDisplay>
					<ChapterTimeStart>00:00:00</ChapterTimeStart>
					<ChapterTimeEnd>0:05:15.470000</ChapterTimeEnd>
				</ChapterAtom>
				<ChapterAtom>
					<ChapterDisplay>
						<ChapterString>Show</ChapterString>
					</ChapterDisplay>
					<ChapterTimeStart>0:05:15.470000</ChapterTimeStart>
					<ChapterTimeEnd>0:29:39.280000</ChapterTimeEnd>
				</ChapterAtom>
			</EditionEntry>
		</Chapters>
		*/
		comskip::checked_format(filename, "%s.mkvtoolnix.chapters", outbasename);
		mkvtoolnix_chapters_file = myfopen(filename, "wb");
		if (!mkvtoolnix_chapters_file)
		{
			fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
			exit(6);
		}
		else
		{
			fprintf(mkvtoolnix_chapters_file, "<?xml version=\"1.0\" encoding=\"ISO - 8859 - 1\"?>\n<Chapters>\n");
		}
	}
	if (output_mkvtoolnix==2)
	{
		/*
		<?xml version="1.0" encoding="ISO-8859-1"?>
		<Chapters>
			<EditionEntry>
				<EditionUID>1</EditionUID>
				<ChapterAtom>
					<ChapterDisplay>
						<ChapterString>Comercial</ChapterString>
					</ChapterDisplay>
					<ChapterTimeStart>00:00:00</ChapterTimeStart>
					<ChapterTimeEnd>0:05:15.470000</ChapterTimeEnd>
				</ChapterAtom>
				<ChapterAtom>
					<ChapterDisplay>
						<ChapterString>Show</ChapterString>
					</ChapterDisplay>
					<ChapterTimeStart>0:05:15.470000</ChapterTimeStart>
					<ChapterTimeEnd>0:29:39.280000</ChapterTimeEnd>
				</ChapterAtom>
			</EditionEntry>
			<EditionEntry>
				<EditionFlagOrdered>1</EditionFlagOrdered>
				<EditionUID>2</EditionUID>
				<ChapterAtom>
					<ChapterDisplay>
						<ChapterString>Show</ChapterString>
					</ChapterDisplay>
					<ChapterFlagEnabled>1</ChapterFlagEnabled>
					<ChapterTimeStart>0:05:15.470000</ChapterTimeStart>
					<ChapterTimeEnd>0:29:39.280000</ChapterTimeEnd>
				</ChapterAtom>
			</EditionEntry>
		</Chapters>
		*/
		comskip::checked_format(filename, "%s.mkvtoolnix.tags", outbasename);
		mkvtoolnix_tags_file = myfopen(filename, "wb");
		if (!mkvtoolnix_tags_file)
		{
			fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
			exit(6);
		}
		else
		{
			fprintf(mkvtoolnix_tags_file, "<?xml version=\"1.0\" encoding=\"ISO - 8859 - 1\"?>\n"\
				"<Tags>\n"
				"\t<Tag>\n"\
				"\t\t<Targets>\n"\
				"\t\t\t<TargetTypeValue>50</TargetTypeValue>\n"\
				"\t\t\t<EditionUID>1</EditionUID>\n"\
				"\t\t</Targets>\n"\
				"\t\t<Simple>\n"\
				"\t\t\t<TagLanguage>eng</TagLanguage>\n"\
				"\t\t\t<Name>TITLE</Name>\n"\
				"\t\t\t<DefaultLanguage>1</DefaultLanguage>\n"\
				"\t\t\t<String>With Commercials</String>\n"\
				"\t\t</Simple>\n"\
				"\t</Tag>\n"\
				"\t<Tag>\n"\
				"\t\t<Targets>\n"\
				"\t\t\t<TargetTypeValue>50</TargetTypeValue>\n"\
				"\t\t\t<EditionUID>2</EditionUID>\n"\
				"\t\t</Targets>\n"\
				"\t\t<Simple>\n"\
				"\t\t\t<TagLanguage>eng</TagLanguage>\n"\
				"\t\t\t<Name>TITLE</Name>\n"\
				"\t\t\t<DefaultLanguage>1</DefaultLanguage>\n"\
				"\t\t\t<String>Without Commercials</String>\n"\
				"\t\t</Simple>\n"\
				"\t</Tag>\n"\
				"</Tags>"
			);
			fclose(mkvtoolnix_tags_file);
		}
	}
}

#define CLOSEOUTFILE(F) { if ((F) && last) fclose(F); }

void OutputCommercialBlock(int i, long prev, long start, long end, bool last)
{
    int s_start, s_end;
    int count;
    double minutes = F2T(frame_count)/60;
    char scomment[80];
    char ecomment[80];

/*
    // Convert from frame array index to (timecode / fps) for external output
    if (prev > 0)
        prev = F2F(prev);
    if (start > 0 && start <= frame_count)
        start = F2F(start);
    if (end > 0 && end <= frame_count)
        end = F2F(end);

    start = max(start,0);
    end = max(end,0);
*/

    s_start = start;
    s_end = end;

    if (sage_minute_bug)
    {
        s_start = (int)(start * (((int)( minutes+0.5))/minutes));
        s_end = (int)(end * (((int)(minutes+0.5))/minutes));
    }
    if (output_default && prev < start /*&& !last */)
    {
        out_file = myfopen(out_filename, "a+");
        if (out_file)
        {
            fprintf(out_file, "%li\t%li\n", F2F(sage_framenumber_bug?s_start/2:s_start), F2F(sage_framenumber_bug?s_end/2:s_end));
            fclose(out_file);
        }
        else  		// If the file can't be opened for writting, wait half a second and try again
        {
            sleep_for_ms(50L);
            out_file = myfopen(out_filename, "a+");
            if (out_file)
            {
                fprintf(out_file, "%li\t%li\n", F2F(sage_framenumber_bug?s_start/2:s_start), F2F(sage_framenumber_bug?s_end/2:s_end));
                fclose(out_file);
            }
            else  	// If the file still can't be opened for writting, give up and exit
            {
                Debug(0, "ERROR writing to %s\n", out_filename);
                exit(103);
            }
        }
    }
    //CLOSEOUTFILE(out_file);

    if (zoomplayer_cutlist_file && prev < start && end - start > 2)
    {
        fprintf(zoomplayer_cutlist_file, "JumpSegment(\"From=%.4f\",\"To=%.4f\")\n", get_frame_pts(start), get_frame_pts(end));
    }
    CLOSEOUTFILE(zoomplayer_cutlist_file);
    if (plist_cutlist_file)
    {
        if (prev < start /* &&!last */)
        {
            // NOTE: we could possibly simplify this to just printing start and end without the math
            fprintf(plist_cutlist_file, "<integer>%ld</integer> <integer>%ld</integer>\n",
                    (unsigned long)(get_frame_pts(start) * 90000), (unsigned long)(get_frame_pts(end)* 90000));
        }
        if (last)
        {
            fprintf(plist_cutlist_file, "</array>\n");
        }
    }
    CLOSEOUTFILE(plist_cutlist_file);

    if (zoomplayer_chapter_file && prev < start && end - start > fps )
    {
//		fprintf(zoomplayer_chapter_file, "AddChapterBySecond(%.4f,Commercial Segment)\nAddChapterBySecond(%.4f,Show Segment)\n", (start) / fps, (end) / fps);
        fprintf(zoomplayer_chapter_file, "AddChapterBySecond(%i,Commercial Segment)\nAddChapterBySecond(%i,Show Segment)\n", (int)(get_frame_pts(start)), (int)(get_frame_pts(end)));
    }
    CLOSEOUTFILE(zoomplayer_chapter_file);

    if (scf_file && prev < start && end - start > fps)
    {
      int rounded_fps = (int)(fps + .5);
      fprintf(scf_file, "CHAPTER%02i=%02li:%02li:%02li.%03li\n", i * 2 + 1, start / (3600 * rounded_fps) % 60, start / (60 * rounded_fps) % 60, start / rounded_fps % 60, start % rounded_fps);
      fprintf(scf_file, "CHAPTER%02iNAME=%s\n", i * 2 + 1, "Commercial starts");
      fprintf(scf_file, "CHAPTER%02i=%02li:%02li:%02li.%03li\n", i * 2 + 2, end / (3600 * rounded_fps) % 60, end / (60 * rounded_fps) % 60, end / rounded_fps % 60, end % rounded_fps);
      fprintf(scf_file, "CHAPTER%02iNAME=%s\n", i * 2 + 2, "Commercial ends");
    }
    CLOSEOUTFILE(scf_file);

    if (ffmeta_file) {
        if (prev != -1 && prev < start) {
            fprintf(ffmeta_file, "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Show Segment\n", (uint64_t)(get_frame_pts(prev+1) * 100), (uint64_t)(get_frame_pts(start) * 100));
        } else if (prev == -1 && start > 5) {
            fprintf(ffmeta_file, "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Show Segment\n", (uint64_t)0, (uint64_t)(get_frame_pts(start) * 100));
        }
        if (start <= 5)
            start = 0;
        if (end - start > 2)
            fprintf(ffmeta_file, "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Commercial Segment\n", (uint64_t)(get_frame_pts(start) * 100), (uint64_t)(get_frame_pts(end) * 100));
    }
    CLOSEOUTFILE(ffmeta_file);

    if (ffsplit_file) {
        if (prev != -1 && prev < start) {
            fprintf(ffsplit_file, "-c copy -ss %.3f -t %.3f segment%03d.ts \n", get_frame_pts(prev+1), get_frame_pts(start) - get_frame_pts(prev+1), i);
        } else if (prev == -1 && start > 5) {
            fprintf(ffsplit_file, "-c copy -ss %.3f -t %.3f segment%03d.ts \n", 0.0, get_frame_pts(start), i);
        }
    }
    CLOSEOUTFILE(ffsplit_file);

    if (vcf_file && prev < start && start - prev > 5 && prev > 0 )
    {
        fprintf(vcf_file, "VirtualDub.subset.AddRange(%li,%li);\n", F2F(prev-1), F2F(start) - F2F(prev));
    }
    CLOSEOUTFILE(vcf_file);

    if (vdr_file && prev < start && end - start > 2)
    {
        if (start < 5)
            start = 0;
        fprintf(vdr_file, "%s start\n",	dblSecondsToStrMinutesFrames(get_frame_pts(start)));
        fprintf(vdr_file, "%s end\n", dblSecondsToStrMinutesFrames(get_frame_pts(end)));
    }
    CLOSEOUTFILE(vdr_file);

    if (projectx_file && prev < start)
    {
        fprintf(projectx_file, "%ld\n", F2F(prev+1));
        fprintf(projectx_file, "%ld\n", F2F(start));
    }
    CLOSEOUTFILE(projectx_file);

    if (avisynth_file && prev < start)
    {
        fprintf(avisynth_file, "%strim(%ld,", (prev < 10 ? "" : " ++ "), F2F(prev+1));
        fprintf(avisynth_file, "%ld)", F2F(start));
    }
    if (avisynth_file && last)
    {
        fprintf(avisynth_file, "\n");
    }
    CLOSEOUTFILE(avisynth_file);

    if (videoredo_file && prev < start && end - start > 2)
    {
        if (i == 0 && demux_pid)
            fprintf(videoredo_file, "<VideoStreamPID>%d\n<AudioStreamPID>%d\n<SubtitlePID1>%d\n", selected_video_pid, selected_audio_pid, selected_subtitle_pid);
        s_start = max(start-videoredo_offset-1,0);
        s_end = max(end - videoredo_offset-1,0);
        fprintf(videoredo_file, "<Cut>%.0f:%.0f\n", get_frame_pts(s_start) * 10000000, get_frame_pts(s_end) * 10000000);
    }
    CLOSEOUTFILE(videoredo_file);

    if (videoredo3_file && prev < start && end - start > 2)
    {
        /*
              <cut Sequence="2" CutStart="00:00:05;10" CutEnd="00:00:20;16" Elapsed="00:00:02;01"> <CutTimeStart>54000113</CutTimeStart> <CutTimeEnd>206400112</CutTimeEnd> </cut>
          */
        if (i == 0 && demux_pid)
            fprintf(videoredo3_file, "<InputPIDList><VideoStreamPID>%d</VideoStreamPID>\n<AudioStreamPID>%d</AudioStreamPID><SubtitlePID1>%d</SubtitlePID1></InputPIDList>\n", selected_video_pid, selected_audio_pid, selected_subtitle_pid);
        s_start = max(start-videoredo_offset-1,0);
        s_end = max(end - videoredo_offset-1,0);
        fprintf(videoredo3_file, "<Cut><CutTimeStart>%.0f</CutTimeStart> <CutTimeEnd>%.0f</CutTimeEnd> </Cut>\n", get_frame_pts(s_start) * 10000000, get_frame_pts(s_end) * 10000000);

    }
    if (videoredo3_file)
    {
        if (last)
        {
//            fprintf(videoredo3_file, "</cutlist></VideoReDoProject>\n");
            fprintf(videoredo3_file, "</CutList>\n");
        }
    }
    CLOSEOUTFILE(videoredo3_file);

    if (btv_file && prev < start)
    {
        strcpy(scomment, dblSecondsToStrMinutes(get_frame_pts(start)));
        strcpy(ecomment, dblSecondsToStrMinutes(get_frame_pts(end)));

        fprintf(btv_file, "<Region><start comment=\"%s\">%.0f</start><end comment=\"%s\">%.0f</end></Region>\n",
                scomment, get_frame_pts(start) * 10000000, ecomment, get_frame_pts(end) * 10000000);
        if (last)
        {
            fprintf(btv_file, "</cutlist>\n");
        }
    }
    CLOSEOUTFILE(btv_file);

    if (edl_file && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        s_start = max(start-edl_offset,0);
        s_end = max(end - edl_offset,0);

        if (demux_pid && enable_mencoder_pts)
        {
            fprintf(edl_file, "%.2f\t%.2f\t%d\n", get_frame_pts(s_start) + F2T(1), get_frame_pts(s_end) + F2T(1), edl_skip_field);
        }
        else
        {
            fprintf(edl_file, "%.2f\t%.2f\t%d\n", get_frame_pts(s_start), get_frame_pts(s_end), edl_skip_field);
        }
    }
    CLOSEOUTFILE(edl_file);

    if (live_file && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        s_start = max(start-edl_offset,0);
        s_end = max(end - edl_offset,0);

        if (demux_pid && enable_mencoder_pts)
        {
            fprintf(live_file, "%.2f\t%.2f\t%d\n", get_frame_pts(s_start) + F2T(1), get_frame_pts(s_end) + F2T(1), edl_skip_field);
        }
        else
        {
            fprintf(live_file, "%.2f\t%.2f\t%d\n", get_frame_pts(s_start), get_frame_pts(s_end), edl_skip_field);
        }
    }
    CLOSEOUTFILE(live_file);

    if (ipodchap_file && prev < start /* &&!last */ && end - start > 2)
    {
//		fprintf(ipodchap_file,"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
        fprintf(ipodchap_file, "CHAPTER%.2i=%s\nCHAPTER%.2iNAME=%d\n", i+2,dblSecondsToStrMinutes(get_frame_pts(end)), i+2, i+2 );
    }
    CLOSEOUTFILE(ipodchap_file);

    if (edlp_file && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        fprintf(edlp_file, "%.2f\t%.2f\t%d\n", get_frame_pts(start) + F2T(1), get_frame_pts(end) + F2T(1), edl_skip_field);
    }
    CLOSEOUTFILE(edlp_file);

    if (bcf_file && prev < start /* &&!last */ && end - start > 2)
    {
        fprintf(bcf_file, "1,%.0f,%.0f\n", get_frame_pts(start) * 1000.0, get_frame_pts(end) * 1000.0);
    }
    CLOSEOUTFILE(bcf_file);

    if (edlx_file && frame)
    {
        if (prev < start /* &&!last */ && end - start > 2)
        {
            fprintf(edlx_file, "<region start=\"%" PRId64 "\" end=\"%" PRId64 "\"/> \n", frame[start].goppos, frame[end].goppos);
        }
        if (last)
        {
            fprintf(edlx_file, "</regionlist>\n");
        }
    }
    CLOSEOUTFILE(edlx_file);

    if (womble_file)
    {
// CLIPLIST: #1 show
// CLIP: morse.mpg
// 6 0 9963
        if (!last)
        {
            if (start - prev > fps)
            {
                fprintf(womble_file, "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, mpegfilename,F2F(prev+1), F2F(start) - F2F(prev));
            }
// CLIPLIST: #2 commercial
// CLIP: morse.mpg
// 6 9963 5196

            fprintf(womble_file, "CLIPLIST: #%i commercial\nCLIP: %s\n6 %li %li\n", i+1, mpegfilename, F2F(start), F2F(end) - F2F(start));
        }
        else
        {
            if (end - prev > 0)
                fprintf(womble_file, "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, mpegfilename, F2F(prev+1), F2F(end) - F2F(prev));
        }
    }
    CLOSEOUTFILE(womble_file);

    if (mls_file)
    {
        if (i == 0)
        {
            count = (commercial_count+1)*2+1;
//            if (commercial[commercial_count].end_frame < frame_count-2)
//                count += 2;
            if (start < fps)
                count -= 1;
            fprintf(mls_file, "[BookmarkList]\nPathName= %s\nVideoStreamID= 0\nFormat= frame\nCount= %d\n", mpegfilename, count);
            if (start >= fps)
                fprintf(mls_file, "%11i 1\n", 0);
        }
        else
            fprintf(mls_file, "%11li 1\n", F2F(prev));
        if (!last)
            fprintf(mls_file, "%11li 0\n", F2F(start));
        else if (start < end - 5) {
            fprintf(mls_file, "%11li 0\n", F2F(start));
            fprintf(mls_file, "%11li 1\n", F2F(end));
        }

    }
    CLOSEOUTFILE(mls_file);

    if (mpgtx_file)
    {
        if (!last)
        {
            if (start - prev > 0)
            {
                fprintf(mpgtx_file, "[%s-",	(prev < fps ? "":intSecondsToStrMinutes( (int)get_frame_pts(prev))));
                fprintf(mpgtx_file, "%s] ", intSecondsToStrMinutes( (int)get_frame_pts(start)));
            }
        }
        else
        {
            if (end - prev > 0)
                fprintf(mpgtx_file, "[%s-]",	intSecondsToStrMinutes( (int)get_frame_pts(prev+1)));
            fprintf(mpgtx_file, "\n");
        }
    }
    CLOSEOUTFILE(mpgtx_file);

    if (dvrcut_file)
    {
        if (start - prev > (int)fps /* && start > 2*fps */)
        {
            fprintf(dvrcut_file, "%s ",	intSecondsToStrMinutes( (int)get_frame_pts(prev)));
            fprintf(dvrcut_file, "%s ", intSecondsToStrMinutes( (int)get_frame_pts(start)));
        }
        if (last)
        {
            fprintf(dvrcut_file, "\n");
        }
    }
    CLOSEOUTFILE(dvrcut_file);

    if (dvrmstb_file)
    {
        if (end - start > 1)
        {
            if (start == 1) start = 0;
            fprintf(dvrmstb_file, "  <commercial start=\"%f\" end=\"%f\" />\n", get_frame_pts(start), get_frame_pts(end));
        }
        if (last)
        {
            fprintf(dvrmstb_file, " </root>\n");
        }
    }
    CLOSEOUTFILE(dvrmstb_file);

    if (mpeg2schnitt_file)
    {
        if (end - start > 1)
        {
            fprintf(mpeg2schnitt_file, "/o%ld ",	F2F(start));
            fprintf(mpeg2schnitt_file, "/i%ld ", F2F(end));
        }
        if (last)
        {
            fprintf(mpeg2schnitt_file, "\n");
        }
    }
    CLOSEOUTFILE(mpeg2schnitt_file);

    if (cuttermaran_file)
    {
        if (prev+1 < start)
        {
            fprintf(cuttermaran_file, "<CutElements refVideoFile=\"0\" StartPosition=\"%li\" EndPosition=\"%li\">\n", F2F(prev+1), F2F(start-1));
            fprintf(cuttermaran_file, "<CurrentFiles refVideoFiles=\"0\" /> <cutAudioFiles refAudioFile=\"1\" /></CutElements>\n");
        }
        if (last)
        {
            if (cuttermaran_options[0] == 0)
                fprintf(cuttermaran_file, "<CmdArgs OutFile=\"%s_clean.m2v\" cut=\"true\" unattended=\"true\" snapToCutPoints=\"true\" closeApp=\"true\" />\n</StateData>\n",inbasename);
            else
                fprintf(cuttermaran_file, "<CmdArgs OutFile=\"%s_clean.m2v\" %s />\n</StateData>\n",inbasename, cuttermaran_options);
        }
    }
    CLOSEOUTFILE(cuttermaran_file);
}


char CompareLetter(int value, int average, int i)
{
    if (cblock[i].reffer == '+' || cblock[i].reffer == '-')
    {
        if (value > 1.2 * average)
        {
            if (cblock[i].reffer == '-')
                return('=');
            else
                return('!');
        }
        if (value < 0.8 * average)
        {
            if (cblock[i].reffer == '-')
                return('!');
            else
                return('=');
        }
    }
    if (value > average)
    {
        return('+');
    }
    if (value < average)
    {
        return('-');
    }
    return('0');

}

void BuildCommercial()
{
    int i;
    commercial_count = -1;
    i = 0;
    while (i < block_count)
    {
        if (cblock[i].score > global_threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > min_show_segment_length) ))
           )
        {
            commercial_count++;
            commercial[commercial_count].start_frame = cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            commercial[commercial_count].end_frame = cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame, commercial[commercial_count].start_frame);
            commercial[commercial_count].start_block = i;
            commercial[commercial_count].end_block = i;
            cblock[i].iscommercial = true;
            i++;
            while (i < block_count && cblock[i].score > global_threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
                  )
            {
                commercial[commercial_count].end_frame = cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame,	commercial[commercial_count].start_frame);
                commercial[commercial_count].end_block = i;
                cblock[i].iscommercial = true;
                i++;
            }
        }
        else
            cblock[i].iscommercial = false;
        i++;
    }
}


bool OutputBlocks(void)
{
    int		i,k;
    long	prev;
    double comlength;
    double	threshold;
    bool	foundCommercials = false;
    bool	deleted = false;

    if (global_threshold >= 0.0)
    {
        threshold = global_threshold;
    }
    else
    {
        threshold = FindScoreThreshold(score_percentile);
    }

    OpenOutputFiles();


    Debug(1, "Threshold used - %.4f", threshold);
    threshold = ceil(threshold * 100) / 100.0;
    Debug(1, "\tAfter rounding - %.4f\n", threshold);

    BuildCommercial();

#ifdef undef
    commercial_count = -1;
    i = 0;
    while (i < block_count)
    {
        if (cblock[i].score > threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
           )
        {
            commercial_count++;
            commercial[commercial_count].start_frame = cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            commercial[commercial_count].end_frame = cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame,	commercial[commercial_count].start_frame);
            commercial[commercial_count].start_block = i;
            commercial[commercial_count].end_block = i;
            cblock[i].iscommercial = true;
            i++;
            while (i < block_count && cblock[i].score > threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) >  (min_show_segment_length) ))
                  )
            {
                commercial[commercial_count].end_frame = cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame, commercial[commercial_count].start_frame);
                commercial[commercial_count].end_block = i;
                cblock[i].iscommercial = true;
                i++;
            }
        }
        else
            cblock[i].iscommercial = false;
        i++;
    }
#endif


    if (!(disable_heuristics & (1 << (5 - 1))))
    {

        if (delete_block_after_commercial > 0)
        {
            for (k = commercial_count; k >= 0; k--)
            {
                i = commercial[k].end_block + 1;
                if (i < block_count && cblock[i].length < delete_block_after_commercial &&
                        cblock[i].score < threshold)
                {
                    Debug(3, "H5 Deleting cblock %i because it is short and comes after a commercial.\n",
                          i);
                    commercial[k].end_frame = cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                    commercial[k].length = F2L(commercial[k].end_frame, commercial[k].start_frame);
                    commercial[k].end_block = i;
                    cblock[i].iscommercial = true;
                    cblock[i].cause |= C_H5;
                    cblock[i].score = 99.99;
                    cblock[i].more |= C_H5;
                }
            }
        }

        if (commercial_count > -1 &&
                commercial[commercial_count].end_block < block_count - 1 &&
                F2L(cblock[block_count-1].f_end, cblock[commercial[commercial_count].end_block].f_end) < min_show_segment_length / 2.0 )
        {
            commercial[commercial_count].end_block = block_count-1;
            commercial[commercial_count].end_frame = cblock[block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame, commercial[commercial_count].start_frame);
            Debug(3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial and its too short.\n",
                  block_count-1, (int)cblock[block_count-1].length);
            cblock[block_count-1].cause |= C_H5;
            cblock[block_count-1].score = 99.99;
            cblock[block_count-1].more |= C_H5;
        }

        if (commercial_count > -1 &&
                commercial[0].start_block == 1 &&
                F2T(cblock[0].f_end) < min_commercialbreak)
        {
            commercial[0].start_block = 0;
            commercial[0].start_frame = cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
            commercial[0].length = F2L(commercial[0].end_frame,	commercial[0].start_frame);
            Debug(3, "H5 Deleting cblock %i of %i seconds because its too short and before first commercial.\n",
                  0, (int)cblock[0].length);
            cblock[0].score = 99.99;
            cblock[0].cause |= C_H5;
            cblock[0].more |= C_H5;

        }

    }


    Debug(2, "\n\n\t---------------------\n\tInitial Commercial List\n\t---------------------\n");
    for (i = 0; i <= commercial_count; i++)
    {
        Debug(
            2,
            "%2i) %6i\t%6i\t%s\n",
            i,
            commercial[i].start_frame,
            commercial[i].end_frame,
            dblSecondsToStrMinutes(commercial[i].length)
        );
    }

#if 1




    if (!(disable_heuristics & (1 << (6 - 1))))
    {

        // Delete too long/short commercials
        for (k = commercial_count; k >= 0; k--)
        {
            if ( (F2T(commercial[k].start_frame) > 1.0   || commercial[k].length < 10.2 /* Sage bug fix */ )
                    &&		// Do not delete too short first or last commercial
                    ((commercial[k].length > max_commercialbreak && k != 0 && k != commercial_count) ||
                     (commercial[k].length < min_commercialbreak)) &&
                    F2L(cblock[block_count-1].f_end, commercial[k].start_frame) > min_commercial_break_at_start_or_end  &&
                    F2T(commercial[k].end_frame) > min_commercial_break_at_start_or_end )
            {
                for (i = commercial[k].start_block; i <= commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because it is part of a too short or too long commercial.\n",
                          i);
                    cblock[i].score = 0;
                    cblock[i].cause |= C_H6;
                    cblock[i].less |= C_H6;
                }
                for (i = k; i < commercial_count; i++)
                {
                    commercial[i] = commercial[i + 1];
                }
                commercial_count--;
                deleted = true;
            }
        }
#ifdef NOTDEF
// keep first seconds
        if (always_keep_first_seconds && commercial_count >= 0)
        {
            k = 0;
            if ( F2T(commercial[k].end_frame) < always_keep_first_seconds)
            {
                for (i = commercial[k].start_block; i <= commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the first %d seconds should always be kept.\n",
                          i, always_keep_first_seconds);
                    cblock[i].score = 0;
                    cblock[i].cause |= C_H6;
                    cblock[i].less |= C_H6;
                }
                for (i = k; i < commercial_count; i++)
                {
                    commercial[i] = commercial[i + 1];
                }
                commercial_count--;
                deleted = true;
            }
        }
        if (always_keep_last_seconds && commercial_count >= 0)
        {
            k = commercial_count;
            if (F2L(cblock[block_count-1].f_end, commercial[k].start_frame) < always_keep_last_seconds)
            {
                for (i = commercial[k].start_block; i <= commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the last %d seconds should always be kept.\n",
                          i, always_keep_last_seconds);
                    cblock[i].score = 0;
                    cblock[i].cause |= C_H6;
                    cblock[i].less |= C_H6;
                }
                for (i = k; i < commercial_count; i++)
                {
                    commercial[i] = commercial[i + 1];
                }
                commercial_count--;
                deleted = true;
            }
        }
#endif

        /*
        		// Delete too short first commercial
        		k = 0;
        		if (commercial_count >= 0 && commercial[k].start_frame < fps &&
        			commercial[k].length < min_commercial_break_at_start_or_end) {
        			for (i = commercial[k].start_block; i <= commercial[k].end_block; i++) {
        				Debug(3, "H6 Deleting block %i because it is part of a too short commercial at the start of the recording.\n",
        					i);
        				cblock[i].score = 0;
        				cblock[i].cause |= C_H6;
        				cblock[i].less |= C_H6;
        			}
        			for (i = k; i < commercial_count; i++) {
        				commercial[i] = commercial[i + 1];
        			}
        			commercial_count--;
        			deleted = true;
        		}
        		// Delete too short last commercial
        		k = commercial_count;
        		if (commercial_count >= 0 && (cblock[block_count-1].f_end - commercial[k].end_frame) < fps &&
        			commercial[k].length < min_commercial_break_at_start_or_end) {
        			for (i = commercial[k].start_block; i <= commercial[k].end_block; i++) {
        				Debug(3, "H6 Deleting block %i because it is part of a too short commercial at the end of the recording.\n",
        					i);
        				cblock[i].score = 0;
        				cblock[i].cause |= C_H6;
        				cblock[i].less |= C_H6;
        			}
        			for (i = k; i < commercial_count; i++) {
        				commercial[i] = commercial[i + 1];
        			}
        			commercial_count--;
        			deleted = true;
        		}
        */
        /*
        	// Delete too short shows
        	for (k = commercial_count-1; k >= 0; k--) {
        		if ( commercial[k+1].start_frame - commercial[k].end_frame < min_show_segment_length / 2.5 * fps ||
        			 (commercial[k].end_frame > after_start &&
        			  commercial[k].end_frame < before_end &&
        			  commercial[k+1].start_frame - commercial[k].end_frame < min_show_segment_length  * fps)
        			) {
        			for (i = commercial[k].end_block+1; i < commercial[k+1].start_block; i++) {
        				cblock[i].score = 99.99;
        				cblock[i].cause |= C_H6;
        				cblock[i].less |= C_H6;
        			}
        			commercial[k].end_block = commercial[k+1].end_block;
        			commercial[k].end_frame = commercial[k+1].end_frame;
        			commercial[k].length = (commercial[k].end_frame - commercial[k].start_frame) / fps;

        			for (i = k+1; i < commercial_count; i++) {
        					commercial[i] = commercial[i + 1];
        			}
        			commercial_count--;
        			deleted = true;
        		}
        	}
        */

    }
    if (delete_show_after_last_commercial &&
            commercial_count > -1 &&
            //	( commercial[commercial_count].end_block == block_count - 2 || commercial[commercial_count].end_block == block_count - 3) &&
            ((delete_show_after_last_commercial == 1 && cblock[commercial[commercial_count].start_block].f_end > before_end) ||
             (delete_show_after_last_commercial > F2L(cblock[block_count-1].f_end, cblock[commercial[commercial_count].start_block].f_start)) )

            &&
            commercial[commercial_count].end_block < block_count-1
       )
    {
        i = commercial[commercial_count].end_block + 1;
        commercial[commercial_count].end_block = block_count-1;
        commercial[commercial_count].end_frame = cblock[block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
        commercial[commercial_count].length = F2L(commercial[commercial_count].end_frame,	commercial[commercial_count].start_frame);
        while (i < block_count)
        {
            Debug(3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial.\n",
                  i, (int)cblock[i].length );
            cblock[i].cause |= C_H5;
            cblock[i].score = 99.99;
            cblock[i].more |= C_H5;
            i++;
        }
    }



    if (delete_show_before_first_commercial &&
            commercial_count > -1 &&
            commercial[0].start_block == 1 &&
            ((delete_show_before_first_commercial == 1 && cblock[commercial[0].end_block].f_end < after_start) ||
             (delete_show_before_first_commercial > F2T(cblock[commercial[0].end_block].f_end)))
       )
    {
        commercial[0].start_block = 0;
        commercial[0].start_frame = cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
        commercial[0].length = F2L(commercial[0].end_frame, commercial[0].start_frame);
        Debug(3, "H5 Deleting cblock %i of %i seconds because it comes before the first commercial.\n",
              0, (int)cblock[0].length);
        cblock[0].score = 99.99;
        cblock[0].cause |= C_H5;
        cblock[0].more |= C_H5;

    }

// keep first seconds
    if (always_keep_first_seconds && commercial_count >= 0)
    {
        k = 0;
        while (commercial_count >= 0 && F2T(commercial[k].end_frame) < always_keep_first_seconds)
        {
            Debug(3, "Deleting commercial block %i because the first %d seconds should always be kept.\n",
                  k, always_keep_first_seconds);
            for (i = k; i <= commercial_count; i++)
            {
                commercial[i] = commercial[i + 1];
            }
            commercial_count--;
            deleted = true;
        }
        if (commercial_count >= 0 && F2T(commercial[k].start_frame ) < always_keep_first_seconds)
        {
            Debug(3, "Shortening commercial block %i because the first %d seconds should always be kept.\n",
                  k, always_keep_first_seconds);
            while (F2T(commercial[k].start_frame ) < always_keep_first_seconds && commercial[k].start_frame < always_keep_first_seconds * fps)
                commercial[k].start_frame++;
        }
    }
    if (always_keep_last_seconds && commercial_count >= 0)
    {
        k = commercial_count;
        while (commercial_count >= 0 && F2L(cblock[block_count-1].f_end, commercial[k].start_frame) < always_keep_last_seconds)
        {
            Debug(3, "Deleting commercial block %i because the last %d seconds should always be kept.\n",
                  k, always_keep_last_seconds);
            commercial_count--;
            k = commercial_count;
            deleted = true;
        }
        if (commercial_count >= 0 && F2L(cblock[block_count-1].f_end, commercial[k].end_frame) < always_keep_last_seconds)
        {
            Debug(3, "Shortening commercial block %i because the last %d seconds should always be kept.\n",
                  k, always_keep_last_seconds);
            while (F2L(cblock[block_count-1].f_end, commercial[k].end_frame) < always_keep_last_seconds && (cblock[block_count-1].f_end - commercial[k].end_frame) < fps * always_keep_last_seconds)
                commercial[k].end_frame--;
        }
    }



    if (deleted)
        Debug(1, "\n\n\t---------------------\n\tFinal Commercial List\n\t---------------------\n");
    else
        Debug(1, "No change\n");
#endif


    // Apply padding
    for (i = 0; i <= commercial_count; i++)
    {
        commercial[i].start_frame += padding*fps - remove_before*fps;
        commercial[i].end_frame -= padding*fps - remove_after*fps;
        if (commercial[i].end_frame > frame_count)
            commercial[i].end_frame = frame_count;
        commercial[i].length += -2*padding + remove_before + remove_after;
    }



    comlength = 0.;
    for (i = 0; i < commercial_count; i++)
    {
        comlength += commercial[i].length;
    }
//	Debug(1, "Total commercial length found: %s\n",	dblSecondsToStrMinutes(comlength));

    if ((zoomplayer_chapter_file) &&
//		(commercial[0].length >= min_commercialbreak) &&
//		(commercial[0].length <= max_commercialbreak) &&
            (commercial[0].start_frame > 5))
    {
        fprintf(zoomplayer_chapter_file, "AddChapter(1,Show Segment)\n");
    }

    if (ffmeta_file) {
        fprintf(ffmeta_file, ";FFMETADATA1\n");
    }

    prev = -1;
    for (i = 0; i <= commercial_count; i++)
    {
//		if ((commercial[i].length >= min_commercialbreak) && (commercial[i].length <= max_commercialbreak))
        {
            foundCommercials = true;
            if (deleted)
                Debug(
                    1,
                    "%i - start: %6i\tend: %6i\t[%6i:%6i]\tlength: %s\n",
                    i + 1,
                    commercial[i].start_frame,
                    commercial[i].end_frame,
                    commercial[i].start_block,
                    commercial[i].end_block,
                    dblSecondsToStrMinutes(commercial[i].length)
                );
            OutputCommercialBlock(i, prev, commercial[i].start_frame, commercial[i].end_frame, (commercial[i].end_frame < frame_count-2 ? false : true));
            prev = commercial[i].end_frame;
        }
    }

    if (commercial[commercial_count].end_frame < frame_count-2)
        OutputCommercialBlock(commercial_count+1, prev, frame_count-2, frame_count-1, true);

    if (output_videoredo)
    {
        comskip::checked_format(filename, "%s.VPrj", outbasename);
        videoredo_file = myfopen(filename, "a+");
        if (videoredo_file)
        {
            for (i = 0; i < block_count; i++)
            {
                fprintf(videoredo_file, "<SceneMarker %d>%.0f\n", i, F2T(max(cblock[i].f_end-videoredo_offset-1,0)) * 10000000);
            }
            fclose(videoredo_file);
        }
    }

    if (output_videoredo3)
    {
        comskip::checked_format(filename, "%s.VPrj", outbasename);
        videoredo3_file = myfopen(filename, "a+");
        if (videoredo3_file)
        {
            fprintf(videoredo3_file, "<SceneList>\n");
            for (i = 0; i < block_count; i++)
            {
// <SceneList>
//   <SceneMarker Sequence="1" Timecode="00:00:56;00">560560112</SceneMarker>
// </SceneList>
                   fprintf(videoredo3_file, "<SceneMarker Sequence=\"%d\" Timecode=\"%s\">%.0f</SceneMarker>\n", i, dblSecondsToStrMinutes(F2T(max(cblock[i].f_end-videoredo_offset-1,0))) , F2T(max(cblock[i].f_end-videoredo_offset-1,0)) * 10000000);
            }
            fprintf(videoredo3_file, "</SceneList>\n");
            fprintf(videoredo3_file, "</VideoReDoProject>\n");
            fclose(videoredo3_file);
        }
    }

    if (output_chapters)
    {
//		comskip::checked_format(filename, "%s.chap", outbasename);
//		chapters_file = myfopen(filename, "a+");
        if (chapters_file)
        {
            for (i = 0; i < block_count; i++)
            {
                fprintf(chapters_file, "%ld\n", cblock[i].f_end);
            }
            fclose(chapters_file);
        }
    }

	if (mkvtoolnix_chapters_file)
	{
		double currentStart = 0;
		char startTimespan[15];
		char endTimespan[15];

		if(output_mkvtoolnix > 0){
			fprintf(mkvtoolnix_chapters_file,"\t<EditionEntry>\n\t\t<EditionUID>1</EditionUID>\n");
			for (i = 0; i < block_count; i++)
            {
				if(i == 0 || (cblock[i-1].iscommercial != cblock[i].iscommercial)){
						currentStart = cblock[i].f_start;
				}
				if(i == i-1 || (cblock[i+1].iscommercial != cblock[i].iscommercial)){
					strcpy(startTimespan, dblSecondsToStrMinutes(get_frame_pts(currentStart)));
					fprintf(mkvtoolnix_chapters_file,
						"\t\t<ChapterAtom>\n"\
						"\t\t\t<ChapterDisplay>\n"\
						"\t\t\t\t<ChapterString>%s</ChapterString>\n"\
						"\t\t\t</ChapterDisplay>\n"\
						"\t\t\t<ChapterTimeStart>%s</ChapterTimeStart>\n"\
						"\t\t</ChapterAtom>\n"
					, cblock[i].iscommercial ? "Commercial" : "Show", startTimespan);
				}
            }
			fprintf(mkvtoolnix_chapters_file,"\t</EditionEntry>\n");
		}
		if(output_mkvtoolnix == 2){
			fprintf(mkvtoolnix_chapters_file,"\t<EditionEntry>\n\t\t<EditionUID>2</EditionUID>\n\t\t<EditionFlagOrdered>1</EditionFlagOrdered>\n");
			for (i = 0; i < block_count; i++)
            {
				if(!cblock[i].iscommercial){
					if(i == 0 || cblock[i-1].iscommercial){
						currentStart = cblock[i].f_start;
					}
					if(i == i-1 || cblock[i+1].iscommercial){
						strcpy(startTimespan, dblSecondsToStrMinutes(get_frame_pts(currentStart)));
						strcpy(endTimespan, dblSecondsToStrMinutes(get_frame_pts(cblock[i].f_end)));
						fprintf(mkvtoolnix_chapters_file,
							"\t\t<ChapterAtom>\n"\
							"\t\t\t<ChapterDisplay>\n"\
							"\t\t\t\t<ChapterString>Show</ChapterString>\n"\
							"\t\t\t</ChapterDisplay>\n"\
							"\t\t\t<ChapterFlagEnabled>1</ChapterFlagEnabled>\n"\
							"\t\t\t<ChapterTimeStart>%s</ChapterTimeStart>\n"\
							"\t\t\t<ChapterTimeEnd>%s</ChapterTimeEnd>\n"\
							"\t\t</ChapterAtom>\n"
						, startTimespan,endTimespan);
					}
				}
            }
			fprintf(mkvtoolnix_chapters_file,"\t</EditionEntry>\n");
		}
		fprintf(mkvtoolnix_chapters_file,"</Chapters>");
		fclose(mkvtoolnix_chapters_file);
	}

    if (reffer_count == -1) {
        reffer_count = commercial_count;
        for (i = 0; i <= commercial_count; i++)
        {
            reffer[i].start_frame = commercial[i].start_frame;
            reffer[i].end_frame = commercial[i].end_frame;
        }
    }

    InputReffer(".ref", false);

    if (output_tuning)
    {
        comskip::checked_format(filename, "%s.tun", workbasename);
        tuning_file = myfopen(filename, "w");
        fprintf(tuning_file,"max_volume=%6i\n", min_volume+200);
        fprintf(tuning_file,"max_avg_brightness=%6i\n", min_brightness_found+5);
        fprintf(tuning_file,"max_commercialbreak=%6i\n", max_logo_gap+10);
        fprintf(tuning_file,"shrink_logo=%.2f\n", logo_overshoot);
        fprintf(tuning_file,"min_show_segment_length=%6i\n", max_nonlogo_block_length+10);
        fprintf(tuning_file,"logo_threshold=%.3f\n", logo_quality);
    }




    if (verbose)
    {
        Debug(1, "\nLogo fraction:              %.4f      %s\n",logoPercentage, ((commDetectMethod & LOGO) ? (reverseLogoLogic? "(Reversed Logo Logic)": "") : "Logo disabled") );
        Debug(1,   "Maximum volume found:       %6i\n", maxi_volume);
        Debug(1,   "Average volume:             %6i\n", avg_volume);
        Debug(1,   "Sound threshold:            %6i\n", max_volume);
        Debug(1,   "Silence threshold:          %6i\n", max_silence);
        Debug(1,   "Minimum volume found:       %6i\n", min_volume);
        Debug(1,   "Average frames with silence:%6i\n", avg_silence);
        Debug(1,   "Black threshold:            %6i\n", max_avg_brightness);
        Debug(1,   "Minimum brightness found:   %6i\n", min_brightness_found);
        Debug(1,   "Minimum bright pixels found:%6i\n", min_hasBright);
        Debug(1,   "Minimum dim level found:    %6i\n", min_dimCount);
        Debug(1,   "Average brightness:         %6i\n", avg_brightness);
        Debug(1,   "Uniformity level:           %6i\n", non_uniformity);
        Debug(1,   "Average non uniformity:     %6i\n", avg_uniform);
        Debug(1,   "Maximum gap between logo's: %6i\n", max_logo_gap);
        Debug(1,   "Suggested logo_threshold:   %.4f\n",logo_quality);
        Debug(1,   "Suggested shrink_logo:	    %.2f\n", logo_overshoot);
        Debug(1,   "Max commercial size found:  %6i\n", max_nonlogo_block_length);
        Debug(1,   "Dominant aspect ratio:      %.4f\n",dominant_ar);
        Debug(1,   "Score threshold:            %.4f\n", threshold);
        Debug(1,   "Framerate:                  %2.3f\n", fps);
        Debug(1,   "Average framerate:          %2.3f\n", avg_fps);

        Debug(1,   "Total commercial length:    %s\n",	dblSecondsToStrMinutes(comlength));
        Debug(1,   "Cut codes:\n");
        Debug(1,   "  F: scene\t c: change\n  A: aspect\t t: cutscene\n  E: exceeds\t l: logo\n  L: logo\t v: volume\n  B: bright\t s: scene_change\n  C: combined\t a: aspect_ratio\n  N: nonstrict\t u: uniform_frame\n  S: strict\t b: black_frame\n  \t\t r: resolution\n");
        Debug(1,   "----------------------------------------------------\n");
        Debug(1,   "Block list after weighing\n----------------------------------------------------\n", threshold);
        Debug(
            1,
            "  #     sbf  bs  be     fs     fe        ts        te       len     sc   scr cmb   ar                   cut    bri logo   vol sil   corr stdev   cc\n"
        );

//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 0, 0,0,0, 0, 0, 0, 0);
//		}




        for (i = 0; i < block_count; i++)
        {
            /*
            			cs[5] = (cblock[i].cause & 16 ? 'b' : ' ');
            			cs[4] = (cblock[i].cause & 8  ? 'u' : ' ');
            			cs[3] = (cblock[i].cause & 32 ? 'a' : ' ');
            			cs[2] = (cblock[i].cause & 4  ? 's' : ' ');
            			cs[1] = (cblock[i].cause & 1  ? 'l' : ' ');
            			cs[0] = (cblock[i].cause & 2  ? 'c' : ' ');
            			cs[6] = 0;
            */

            Debug(
                1,
                "%3i:%c%c %4i %3i %3i %6i %6i %8.2fs %8.2fs %8.2fs %6.2f %5.2f %3i %4.2f %s %4i%c %4.2f %4i%c %2i%c %6.3f %5i %-10s",
                i,
                CheckFramesForCommercial(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail),
                CheckFramesForReffer(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail),
                cblock[i].bframe_count,
                cblock[i].b_head,
                cblock[i].b_tail,
                cblock[i].f_start,
                cblock[i].f_end,
                get_frame_pts(cblock[i].f_start),
                get_frame_pts(cblock[i].f_end),
                cblock[i].length,
                cblock[i].score,
//				cblock[i].schange_count,
                cblock[i].schange_rate,
                cblock[i].combined_count,
                cblock[i].ar_ratio,
                CauseString(cblock[i].cause),
                cblock[i].brightness,
                CompareLetter(cblock[i].brightness,avg_brightness,i),
                cblock[i].logo,
                cblock[i].volume,
                CompareLetter(cblock[i].volume,avg_volume,i),
                cblock[i].silence,
                CompareLetter(cblock[i].silence,avg_silence,i),
                0.0 /*cblock[i].correlation */ ,
                cblock[i].stdev,
                CCTypeToStr(cblock[i].cc_type)
            );
            if (commDetectMethod & LOGO)
            {
//				if (CheckFramesForLogo(cblock[i].f_start, cblock[i].f_end)) {
//					Debug(1, "\tLogo Present\n");
//				} else {
                Debug(1, "\n");
//				}
            }
            else
            {
                Debug(1, "\n");
            }
        }

        OutputAspect();
        OutputTraining();



//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 100, 0,0,0, 0, 0, 0, 0);
//		}

    }

//	OutputCleanMpg();
//	OutputDebugWindow(false,0);
    return (foundCommercials);
}

void OutputStrict(double len, double delta, double tol)
{
//return;
    if (output_training && !training_file)
    {
        training_file = myfopen("strict.csv", "a+");
//		fprintf(training_file, "// score, length, fraction, position,combined, ar error, logo, strict \n");
    }
    if (training_file)
        fprintf(training_file, "%+f,%+f,%+f, %s\n", len,delta, tol, inbasename);
}




void OutputTraining()
{
    int i;
//	return;
    if (!output_training)
        return;
    training_file = myfopen("comskip.csv", "a+");

#ifdef WRITEPATTERN
    r = (reffer[0].start_frame/fps < 30.0 ? reffer_count: reffer_count+1);
    if (reffer[0].start_frame/fps < 30.0)
        s = reffer[0].end_frame;
    else
        s = 0;
    fprintf(training_file, "\"%s\",%f,%d,", inbasename,  (reffer[reffer_count].start_frame - s)/fps, r);
    for (i = 0; i < 40; i++)
    {
        if (i <= reffer_count)
        {
            if (i == 0)
                e = 0;
            else
                e = reffer[i-1].end_frame;
            if (i == reffer_count)
                s = 0;
            else
                s = (reffer[i].end_frame - reffer[i].start_frame);
            if (i > 0)
                fprintf(training_file, "%f,%f,", (reffer[i].start_frame-e)/fps, s/fps);
            else
            {
                if (reffer[i].start_frame/fps > 30.0)
                    fprintf(training_file, "%f,%f, %f,%f,", 0.0, 0.0, (reffer[i].start_frame-e)/fps,s/fps);
                else
                    fprintf(training_file, "%f,%f,", (reffer[i].start_frame-e)/fps,s/fps);
            }
        }
        else
        {
            fprintf(training_file, "%f,%f,", 0.0, 0.0);
        }
    }
    fprintf(training_file, "0\n", inbasename);


    r = (commercial[0].start_frame/fps < 30.0 ? commercial_count: commercial_count+1);
    if (commercial[0].start_frame/fps < 30.0)
        s = commercial[0].end_frame;
    else
        s = 0;
    fprintf(training_file, "\"%s\",%f,%d,", inbasename,  (commercial[commercial_count].start_frame - s)/fps, r);
    for (i = 0; i < 40; i++)
    {
        if (i <= commercial_count)
        {
            if (i == 0)
                e = 0;
            else
                e = commercial[i-1].end_frame;
            if (i == commercial_count)
                s = 0;
            else
                s = (commercial[i].end_frame - commercial[i].start_frame);
            if (i > 0)
                fprintf(training_file, "%f,%f,", (commercial[i].start_frame-e)/fps, s/fps);
            else
            {
                if (commercial[i].start_frame/fps > 30.0)
                    fprintf(training_file, "%f,%f, %f,%f,", 0.0, 0.0, (commercial[i].start_frame-e)/fps,s/fps);
                else
                    fprintf(training_file, "%f,%f,", (commercial[i].start_frame-e)/fps,s/fps);
            }
        }
        else
        {
            fprintf(training_file, "%f,%f,", 0.0, 0.0);
        }
    }
    fprintf(training_file, "0\n", inbasename);

#else

#define TRAINING_LAYOUT	"%3d,%c,%c,%7.2f,%7.2f,%7.2f,%7.2f,%7.2f,%5.2f,%5.2f,\"%10s\",\"%10s\",\"%10s\",\"%s\"\n"

    fprintf(training_file, "block, cm,rf, score, length, start, end, fromend ar, logo, cause, less, more\n");

    for (i = 0; i < block_count; i++)
    {
        if (output_training)
        {
            fprintf(training_file, TRAINING_LAYOUT,
                    i,
                    CheckFramesForCommercial(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail),
                    CheckFramesForReffer(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail),
                    cblock[i].score,
                    cblock[i].length,
                    F2T(cblock[i].f_start),
                    F2T(cblock[i].f_end),
                    F2L(cblock[block_count-1].f_end, cblock[i].f_end),
                    cblock[i].ar_ratio,
                    cblock[i].logo,
                    CauseString(cblock[i].cause),
                    CauseString(cblock[i].less),
                    CauseString(cblock[i].more),
                    inbasename);

        }
    }
#endif

}


unsigned char MPEG2SysHdr[] = {0x00, 0x00, 0x01, 0xBB, 00, 0x12, 0x80, 0x8E, 0xD3, 0x04, 0xE1, 0x7F, 0xB9, 0xE0, 0xE0, 0xB8, 0xC0, 0x54, 0xBD, 0xE0, 0x3A, 0xBF, 0xE0, 0x02};

bool OutputCleanMpg()
{
    int inf, outf;
    int i,j,c;
    int64_t startpos=0, endpos=0, begin=0;
    int len;
    int prevperc,curperc;
    char *Buf;//[65536];
#ifndef _WIN32
    FILE *infile;
#endif

    bool firstbl = true;
#define BufSize 1<<22

    //long dwPackStart=0xBA010000;

    if (outputdirname[0] == 0) return(true);

    if (!(Buf=(char*)malloc(BufSize))) return(false);

#ifdef _WIN32
    outf = _creat(outputdirname, _S_IREAD | _S_IWRITE);
    if(outf<0) return(false);
    inf = _open(mpegfilename, _O_RDONLY | _O_BINARY);
#else
    outf = open(outputdirname, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if(outf<0)
        return(false);

    infile=myfopen(mpegfilename,"rb");
    inf = fileno(infile);
#endif

    /*
    	if (_lseeki64(Infile[File_Limit-1], process.leftlba*BUFFER_SIZE,SEEK_SET)!= -1L)
    	{

    		j = _read(Infile[File_Limit-1], Buf, BufSize);
    		if (j>=BUFFER_SIZE)
    		{
    			for(i=0; i<(j-4); i++)
    			{
    				if(*((UNALIGNED DWORD*)(Buf+i)) == dwPackStart)
    				{
    					startpos = (process.leftlba*BUFFER_SIZE) + i;
    					endpos = process.total;

    					if (_lseeki64(Infile[File_Limit-1], process.rightlba*BUFFER_SIZE,SEEK_SET)!= -1L)
    					{
    						j = _read(Infile[File_Limit-1], Buf, BufSize);
    						if (j>=BUFFER_SIZE)
    						{
    							for(i=0; i<(j-4); i++)
    							{
    								if(*((UNALIGNED DWORD*)(Buf+i)) == dwPackStart)
    								{
    									endpos = (process.rightlba*BUFFER_SIZE) + i;
    									break;
    								}
    							}

    						}

    					}

    					*/

    startpos = frame[1].goppos;

    for (c=0; c<=commercial_count; c++)
    {

        endpos = frame[commercial[c].start_frame].goppos;
#ifdef _WIN32
        _lseeki64(inf, startpos,SEEK_SET);
#else
        fseeko(infile, startpos,SEEK_SET);
#endif

        begin = startpos;
        prevperc = 0;

        while (startpos<endpos)
        {
            len = (int)endpos-startpos;
            if(len>BufSize) len = BufSize;//sizeof(Buf);
            i = _read(inf, Buf, (unsigned int)len);
            if(i<=0)
            {
                //				MessageBox(hWnd, "Source read error.         ", "Oops...", MB_ICONSTOP | MB_OK);
                return(false);
            }
            j=0;

            if (firstbl)
            {
                firstbl=false;
                j = 14 + (Buf[13] & 7);
#ifdef _WIN32
                if (*((UNALIGNED DWORD*)(Buf+j)) == 0xBB010000)
#else
                if (*((uint32_t*)(Buf+j)) == 0xBB010000)
#endif
                    j=0;
                else
                {
                    _write(outf, Buf, j);
                    _write(outf, MPEG2SysHdr, sizeof(MPEG2SysHdr));
                }
            }

            if(_write(outf, Buf+j, i-j)<=0)
            {
                //				MessageBox(hWnd, "Write error.         ", "Oops...", MB_ICONSTOP | MB_OK);
                return(false);
            }

            if (i!=len)
            {
                //				MessageBox(hWnd, "Something strange happened. Aborting.         ", "Oops...", MB_ICONSTOP | MB_OK);
                return(false);
            }
            startpos +=len;

            curperc = (int)(((startpos-begin)*100)/(endpos-begin));

            if (curperc != prevperc)
            {
                //				SendMessage(hBar, PBM_SETPOS, DWORD(curperc),0);
                prevperc=curperc;
            }

        }
        startpos = frame[commercial[c].end_frame].goppos;

    }
    _close(outf);
    free(Buf);
    return(true);
}
