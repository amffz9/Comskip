#include "exit_requested.h"
#include "checked_format.h"
#include "xml_filename.h"
#include "edl.h"
#include <filesystem>
#include <sstream>
#include <vector>
#include "legacy_detection.h"

namespace {
void append_edl_record(RecordingContext& context, FILE* destination, long start, long end,
                       comskip::output::EdlVariant variant)
{
    using namespace comskip::output;
    const OutputOptions options{context.settings.edl_offset, context.settings.edl_skip_field,
                                context.state.demux_pid && context.settings.enable_mencoder_pts, variant};
    std::vector<Seconds> timestamps;
    MediaDescription media{context.settings.fps};
    if (!context.state.frame.empty() && context.state.frame_count > 1) {
        auto first = static_cast<FrameIndex>(start < 5 ? 0 : start);
        auto last = static_cast<FrameIndex>(end);
        if (variant == EdlVariant::standard) {
            first = std::max<FrameIndex>(first - options.frame_offset, 0);
            last = std::max<FrameIndex>(last - options.frame_offset, 0);
        }
        first = std::clamp<FrameIndex>(first, 1, context.state.frame_count - 1);
        last = std::clamp<FrameIndex>(last, 1, context.state.frame_count - 1);
        timestamps.reserve(static_cast<std::size_t>(last - first + 1));
        for (auto index = first; index <= last; ++index)
            timestamps.emplace_back(context.state.frame[index].pts);
        media.timestamps = timestamps;
        media.first_frame = first;
        media.first_frame_timestamp = Seconds{get_frame_pts(context, 1)};
    }
    const CommercialInterval interval{start, end};
    std::ostringstream serialized;
    write_edl(serialized, std::span{&interval, 1}, media, options);
    const auto text = serialized.str();
    if (fwrite(text.data(), 1, text.size(), destination) != text.size())
        throw std::ios_base::failure("Failed writing commercial EDL output");
}
}

void OpenOutputFiles(RecordingContext& context)
{
    char	tempstr[MAX_PATH];
    char	cwd[MAX_PATH];

    if (context.settings.output_default)
    {
        context.state.out_file.reset(myfopen(context.state.out_filename, "w"));
        if (!context.state.out_file.get())
        {
            sleep_for_ms(50L);
            context.state.out_file.reset(myfopen(context.state.out_filename, "w"));
            if (!context.state.out_file.get())
            {
                Debug(context, 0, "ERROR writing to %s\n", context.state.out_filename);
                comskip::request_exit(103);
            }
        }
        fprintf(context.state.out_file.get(), "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",F2F(context.state.frame_count-1), (int)(context.settings.fps*100));
        context.state.out_file.reset();
    }

    if (context.settings.output_chapters)
    {
        comskip::checked_format(context.state.filename, "%s.chap", context.state.outbasename);
        context.state.chapters_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.chapters_file.get())
        {
            sleep_for_ms(50L);
            context.state.chapters_file.reset(myfopen(context.state.filename, "w"));
            if (!context.state.chapters_file.get())
            {
                Debug(context, 0, "ERROR writing to %s\n", context.state.filename);
                comskip::request_exit(103);
            }
        }
        fprintf(context.state.chapters_file.get(), "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",context.state.frame_count-1, (int)(context.settings.fps*100));
    }

    if (context.settings.output_zoomplayer_cutlist)
    {
        comskip::checked_format(context.state.filename, "%s.cut", context.state.outbasename);
        context.state.zoomplayer_cutlist_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.zoomplayer_cutlist_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_zoomplayer_cutlist = true;
//			fclose(zoomplayer_cutlist_file);
        }
    }
    if (context.settings.output_plist_cutlist)
    {
        comskip::checked_format(context.state.filename, "%s.plist", context.state.outbasename);
        context.state.plist_cutlist_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.plist_cutlist_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_plist_cutlist = true;
            fprintf(context.state.plist_cutlist_file.get(), "<array>\n");
//			fclose(plist_cutlist_file);
        }
    }

    if (context.settings.output_incommercial)
    {
        comskip::checked_format(context.state.filename, "%s.incommercial", context.state.workbasename);
        context.state.incommercial_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.incommercial_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        fprintf(context.state.incommercial_file.get(), "0\n");
        context.state.incommercial_file.reset();
    }




    if (context.settings.output_zoomplayer_chapter)
    {
        comskip::checked_format(context.state.filename, "%s.chp", context.state.outbasename);
        context.state.zoomplayer_chapter_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.zoomplayer_chapter_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_zoomplayer_chapter = true;
//			fclose(zoomplayer_chapter_file);
        }
    }

    if (context.settings.output_scf)
    {
        comskip::checked_format(context.state.filename, "%s.scf", context.state.outbasename);
        context.state.scf_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.scf_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_scf = true;
        }
    }

    if (context.settings.output_edl)
    {
        comskip::checked_format(context.state.filename, "%s.edl", context.state.outbasename);
        context.state.edl_file.reset(myfopen(context.state.filename, "wb"));
        if (!context.state.edl_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_edl = true;
        }
    }

    if (context.settings.output_ffmeta)
    {
        comskip::checked_format(context.state.filename, "%s.ffmeta", context.state.outbasename);
        context.state.ffmeta_file.reset(myfopen(context.state.filename, "wb"));
        if (!context.state.ffmeta_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_ffmeta = true;
        }
    }

    if (context.settings.output_ffsplit)
    {
        comskip::checked_format(context.state.filename, "%s.ffsplit", context.state.outbasename);
        context.state.ffsplit_file.reset(myfopen(context.state.filename, "wb"));
        if (!context.state.ffsplit_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_ffsplit = true;
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
            comskip::request_exit(6);
        }
        else
        {
            output_live = true;
        }
    }
*/
    if (context.settings.output_ipodchap)
    {
        comskip::checked_format(context.state.filename, "%s.chap", context.state.outbasename);
        context.state.ipodchap_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.ipodchap_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_ipodchap = true;
        }
        fprintf(context.state.ipodchap_file.get(),"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
    }

    if (context.settings.output_edlp)
    {
        comskip::checked_format(context.state.filename, "%s.edlp", context.state.outbasename);
        context.state.edlp_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.edlp_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_edlp = true;
        }
    }


    if (context.settings.output_bsplayer)
    {
        comskip::checked_format(context.state.filename, "%s.bcf", context.state.outbasename);
        context.state.bcf_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.bcf_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_bsplayer = true;
        }
    }

    if (context.settings.output_edlx)
    {
        comskip::checked_format(context.state.filename, "%s.edlx", context.state.outbasename);
        context.state.edlx_file.reset(myfopen(context.state.filename, "w"));
        if (!context.state.edlx_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_edlx = true;
            fprintf(context.state.edlx_file.get(), "<regionlist units=\"bytes\" mode=\"exclude\"> \n");
        }
    }


    if (context.settings.output_videoredo && !context.settings.output_videoredo3)
    {
//<Version>2
//<Filename>G:\comskip79_46\mpg\MXC_20060518_00000030.mpg
//<Cut>4255584667:5666994667
//<Cut>8590582000:11001991000
//<SceneMarker 0>797115333
//<SceneMarker 1>1083729555
//<SceneMarker 2>4254502333
//<SceneMarker 3>4708947222

        comskip::checked_format(context.state.filename, "%s.VPrj", context.state.outbasename);
        context.state.videoredo_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.videoredo_file.get())
        {
            if (context.state.mpegfilename[1] == ':' || context.state.mpegfilename[0] == PATH_SEPARATOR)
            {
                fprintf(context.state.videoredo_file.get(), "<Version>2\n<Filename>%s\n", context.state.mpegfilename);
            }
            else
            {
                _getcwd(cwd, 256);
                fprintf(context.state.videoredo_file.get(), "<Version>2\n<Filename>%s%c%s\n", cwd, PATH_SEPARATOR, context.state.mpegfilename);
            }
            if (context.state.is_h264)
            {
                fprintf(context.state.videoredo_file.get(), "<MPEG Stream Type>4\n");
            }

//			fclose(videoredo_file);
            context.settings.output_videoredo = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }
    if (context.settings.output_videoredo3)
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

        comskip::checked_format(context.state.filename, "%s.VPrj", context.state.outbasename);
        context.state.videoredo3_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.videoredo3_file.get())
        {
            if (context.state.mpegfilename[1] == ':' || context.state.mpegfilename[0] == PATH_SEPARATOR)
            {
                fprintf(context.state.videoredo3_file.get(), "<VideoReDoProject Version=\"3\">\n<Filename>%s</Filename><CutList>\n", comskip::output::escape_xml_filename(context.state.mpegfilename).c_str());
            }
            else
            {
                const auto directory = std::filesystem::current_path().u8string();
                const auto full_filename = std::string(reinterpret_cast<const char*>(directory.data()), directory.size()) + PATH_SEPARATOR + context.state.mpegfilename;
                fprintf(context.state.videoredo3_file.get(), "<VideoReDoProject Version=\"3\">\n<Filename>%s</Filename><CutList>\n", comskip::output::escape_xml_filename(full_filename).c_str());
            }
//              if (is_h264) {
            //                 fprintf(videoredo3_file, "<MPEG Stream Type>4\n");
            //          }

//			fclose(videoredo3_file);
            context.settings.output_videoredo3 = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_btv)
    {
        comskip::checked_format(context.state.filename, "%s.chapters.xml", context.state.mpegfilename);
        context.state.btv_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.btv_file.get())
        {
            fprintf(context.state.btv_file.get(), "<cutlist>\n");
//			fclose(btv_file);
            context.settings.output_btv = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_cuttermaran)
    {
        comskip::checked_format(context.state.filename, "%s.cpf", context.state.outbasename);
        context.state.cuttermaran_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.cuttermaran_file.get())
        {
            if (context.state.mpegfilename[1] == ':' || context.state.mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, context.state.inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, context.state.inbasename);
            }
            fprintf(context.state.cuttermaran_file.get(), "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
            fprintf(context.state.cuttermaran_file.get(), "<StateData xmlns=\"http://cuttermaran.kickme.to/StateData.xsd\">\n");
            fprintf(context.state.cuttermaran_file.get(), "<usedVideoFiles FileID=\"0\" FileName=\"%s.M2V\" />\n",context.state.inbasename);
            fprintf(context.state.cuttermaran_file.get(), "<usedAudioFiles FileID=\"1\" FileName=\"%s.mp2\" StartDelay=\"0\" />\n",context.state.inbasename);
//			fclose(cuttermaran_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_vcf)
    {
        comskip::checked_format(context.state.filename, "%s.vcf", context.state.outbasename);
        context.state.vcf_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.vcf_file.get())
        {
            if (context.state.mpegfilename[1] == ':' || context.state.mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, context.state.inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, context.state.inbasename);
            }
            fprintf(context.state.vcf_file.get(), "VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
//			fclose(vcf_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_vdr)
    {
        comskip::checked_format(context.state.filename, "%s.vdr", context.state.outbasename);
        context.state.vdr_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.vdr_file.get())
        {
            if (context.state.mpegfilename[1] == ':' || context.state.mpegfilename[0] == PATH_SEPARATOR)
            {
                strcpy(tempstr, context.state.inbasename);
            }
            else
            {
                _getcwd(cwd, 256);
                sprintf(tempstr, "%s%c%s", cwd, PATH_SEPARATOR, context.state.inbasename);
            }
//			fprintf(vdr_file, "VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
//			fclose(vdr_file);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_projectx)
    {
        comskip::checked_format(context.state.filename, "%s.Xcl", context.state.mpegfilename);
        context.state.projectx_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.projectx_file.get())
        {
            fprintf(context.state.projectx_file.get(), "CollectionPanel.CutMode=2\n");
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_avisynth)
    {
        comskip::checked_format(context.state.filename, "%s.avs", context.state.mpegfilename);
        context.state.avisynth_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.avisynth_file.get())
        {
            if (context.settings.avisynth_options.c_str()[0] == 0)
                fprintf(context.state.avisynth_file.get(), "LoadPlugin(\"MPEG2Dec3.dll\") \nMPEG2Source(\"%s\")\n", context.state.mpegfilename);
            else
                fprintf(context.state.avisynth_file.get(), context.settings.avisynth_options.c_str(), context.state.mpegfilename);

        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_womble)
    {
        comskip::checked_format(context.state.filename, "%s.wme", context.state.outbasename);
        context.state.womble_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.womble_file.get())
        {
//			fclose(womble_file);
            context.settings.output_womble = true;
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_mls)
    {
        comskip::checked_format(context.state.filename, "%s.mls", context.state.outbasename);
        context.state.mls_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.mls_file.get())
        {
//			fclose(mls_file);
            context.settings.output_mls = true;
//[BookmarkList]
//PathName= C:\VidTst\Will - Grace - Secrets - Lays.mpg
//VideoStreamID= 224
//Format= frame
//Count= 19

        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_mpgtx)
    {
        comskip::checked_format(context.state.filename, "%s_mpgtx.bat", context.state.outbasename);
        context.state.mpgtx_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.mpgtx_file.get())
        {
//			fclose(mpgtx_file);
            context.settings.output_mpgtx = true;
            fprintf(context.state.mpgtx_file.get(), "mpgtx.exe -j -f -o \"%s%s\" \"%s\" ", context.state.mpegfilename, ".clean", context.state.mpegfilename);
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_dvrcut)
    {
        comskip::checked_format(context.state.filename, "%s_dvrcut.bat", context.state.outbasename);
        context.state.dvrcut_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.dvrcut_file.get())
        {
//			fclose(dvrcut_file);
            if (context.settings.dvrcut_options.c_str()[0] == 0)
                fprintf(context.state.dvrcut_file.get(), "dvrcut \"%%1\" \"%%2\" ");
            else
                fprintf(context.state.dvrcut_file.get(), context.settings.dvrcut_options.c_str(), context.state.inbasename, context.state.inbasename, context.state.inbasename  );
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_dvrmstb)
    {
        comskip::checked_format(context.state.filename, "%s.xml", context.state.outbasename);
        context.state.dvrmstb_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.dvrmstb_file.get())
        {
//			fclose(dvrmstb_file);
            fprintf(context.state.dvrmstb_file.get(), "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<root>\n");
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_mpeg2schnitt)
    {
        comskip::checked_format(context.state.filename, "%s_mpeg2schnitt.bat", context.state.inbasename);
        context.state.mpeg2schnitt_file.reset(myfopen(context.state.filename, "w"));
        if (context.state.mpeg2schnitt_file.get())
        {
//			fclose(mpeg2schnitt_file);
            context.settings.output_mpgtx = true;
// Mpeg2Schnitt.exe %1.m2v /R29.97 /o250 /i550 /o3210 /i4000 /S /E /Z %2.m2v
            if (context.settings.mpeg2schnitt_options.c_str()[0] == 0)
                fprintf(context.state.mpeg2schnitt_file.get(), "mpeg2schnitt.exe /S /E /R%5.2f  /Z \"%s\" \"%s\" ", context.settings.fps, "%2", "%1");
            else
                fprintf(context.state.mpeg2schnitt_file.get(), "%s ", context.settings.mpeg2schnitt_options.c_str());
        }
        else
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
    }
        if (context.settings.output_mkvtoolnix>0)
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
        comskip::checked_format(context.state.filename, "%s.mkvtoolnix.chapters", context.state.outbasename);
        context.state.mkvtoolnix_chapters_file.reset(myfopen(context.state.filename, "wb"));
        if (!context.state.mkvtoolnix_chapters_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            fprintf(context.state.mkvtoolnix_chapters_file.get(), "<?xml version=\"1.0\" encoding=\"ISO - 8859 - 1\"?>\n<Chapters>\n");
        }
    }
    if (context.settings.output_mkvtoolnix==2)
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
        comskip::checked_format(context.state.filename, "%s.mkvtoolnix.tags", context.state.outbasename);
        context.state.mkvtoolnix_tags_file.reset(myfopen(context.state.filename, "wb"));
        if (!context.state.mkvtoolnix_tags_file.get())
        {
            fprintf(stderr, "%s - could not create file %s\n", strerror(errno), context.state.filename);
            comskip::request_exit(6);
        }
        else
        {
            fprintf(context.state.mkvtoolnix_tags_file.get(), "<?xml version=\"1.0\" encoding=\"ISO - 8859 - 1\"?>\n"\
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
            context.state.mkvtoolnix_tags_file.reset();
        }
    }
}

#define CLOSEOUTFILE(F) do { if (last) (F).reset(); } while (false)

void OutputCommercialBlock(RecordingContext& context, int i, long prev, long start, long end, bool last)
{
    int s_start, s_end;
    int count;
    double minutes = F2T(context.state.frame_count)/60;
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

    if (context.settings.sage_minute_bug)
    {
        s_start = (int)(start * (((int)( minutes+0.5))/minutes));
        s_end = (int)(end * (((int)(minutes+0.5))/minutes));
    }
    if (context.settings.output_default && prev < start /*&& !last */)
    {
        context.state.out_file.reset(myfopen(context.state.out_filename, "a+"));
        if (context.state.out_file.get())
        {
            fprintf(context.state.out_file.get(), "%li\t%li\n", F2F(context.settings.sage_framenumber_bug?s_start/2:s_start), F2F(context.settings.sage_framenumber_bug?s_end/2:s_end));
            context.state.out_file.reset();
        }
        else  		// If the file can't be opened for writting, wait half a second and try again
        {
            sleep_for_ms(50L);
            context.state.out_file.reset(myfopen(context.state.out_filename, "a+"));
            if (context.state.out_file.get())
            {
                fprintf(context.state.out_file.get(), "%li\t%li\n", F2F(context.settings.sage_framenumber_bug?s_start/2:s_start), F2F(context.settings.sage_framenumber_bug?s_end/2:s_end));
                context.state.out_file.reset();
            }
            else  	// If the file still can't be opened for writting, give up and exit
            {
                Debug(context, 0, "ERROR writing to %s\n", context.state.out_filename);
                comskip::request_exit(103);
            }
        }
    }
    //CLOSEOUTFILE(context.state.out_file);

    if (context.state.zoomplayer_cutlist_file.get() && prev < start && end - start > 2)
    {
        fprintf(context.state.zoomplayer_cutlist_file.get(), "JumpSegment(\"From=%.4f\",\"To=%.4f\")\n", get_frame_pts(context, start), get_frame_pts(context, end));
    }
    CLOSEOUTFILE(context.state.zoomplayer_cutlist_file);
    if (context.state.plist_cutlist_file.get())
    {
        if (prev < start /* &&!last */)
        {
            // NOTE: we could possibly simplify this to just printing start and end without the math
            fprintf(context.state.plist_cutlist_file.get(), "<integer>%ld</integer> <integer>%ld</integer>\n",
                    (unsigned long)(get_frame_pts(context, start) * 90000), (unsigned long)(get_frame_pts(context, end)* 90000));
        }
        if (last)
        {
            fprintf(context.state.plist_cutlist_file.get(), "</array>\n");
        }
    }
    CLOSEOUTFILE(context.state.plist_cutlist_file);

    if (context.state.zoomplayer_chapter_file.get() && prev < start && end - start > context.settings.fps )
    {
//		fprintf(zoomplayer_chapter_file, "AddChapterBySecond(%.4f,Commercial Segment)\nAddChapterBySecond(%.4f,Show Segment)\n", (start) / fps, (end) / fps);
        fprintf(context.state.zoomplayer_chapter_file.get(), "AddChapterBySecond(%i,Commercial Segment)\nAddChapterBySecond(%i,Show Segment)\n", (int)(get_frame_pts(context, start)), (int)(get_frame_pts(context, end)));
    }
    CLOSEOUTFILE(context.state.zoomplayer_chapter_file);

    if (context.state.scf_file.get() && prev < start && end - start > context.settings.fps)
    {
      int rounded_fps = (int)(context.settings.fps + .5);
      fprintf(context.state.scf_file.get(), "CHAPTER%02i=%02li:%02li:%02li.%03li\n", i * 2 + 1, start / (3600 * rounded_fps) % 60, start / (60 * rounded_fps) % 60, start / rounded_fps % 60, start % rounded_fps);
      fprintf(context.state.scf_file.get(), "CHAPTER%02iNAME=%s\n", i * 2 + 1, "Commercial starts");
      fprintf(context.state.scf_file.get(), "CHAPTER%02i=%02li:%02li:%02li.%03li\n", i * 2 + 2, end / (3600 * rounded_fps) % 60, end / (60 * rounded_fps) % 60, end / rounded_fps % 60, end % rounded_fps);
      fprintf(context.state.scf_file.get(), "CHAPTER%02iNAME=%s\n", i * 2 + 2, "Commercial ends");
    }
    CLOSEOUTFILE(context.state.scf_file);

    if (context.state.ffmeta_file.get()) {
        if (prev != -1 && prev < start) {
            fprintf(context.state.ffmeta_file.get(), "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Show Segment\n", (uint64_t)(get_frame_pts(context, prev+1) * 100), (uint64_t)(get_frame_pts(context, start) * 100));
        } else if (prev == -1 && start > 5) {
            fprintf(context.state.ffmeta_file.get(), "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Show Segment\n", (uint64_t)0, (uint64_t)(get_frame_pts(context, start) * 100));
        }
        if (start <= 5)
            start = 0;
        if (end - start > 2)
            fprintf(context.state.ffmeta_file.get(), "[CHAPTER]\nTIMEBASE=1/100\nSTART=%" PRIu64 "\nEND=%" PRIu64 "\ntitle=Commercial Segment\n", (uint64_t)(get_frame_pts(context, start) * 100), (uint64_t)(get_frame_pts(context, end) * 100));
    }
    CLOSEOUTFILE(context.state.ffmeta_file);

    if (context.state.ffsplit_file.get()) {
        if (prev != -1 && prev < start) {
            fprintf(context.state.ffsplit_file.get(), "-c copy -ss %.3f -t %.3f segment%03d.ts \n", get_frame_pts(context, prev+1), get_frame_pts(context, start) - get_frame_pts(context, prev+1), i);
        } else if (prev == -1 && start > 5) {
            fprintf(context.state.ffsplit_file.get(), "-c copy -ss %.3f -t %.3f segment%03d.ts \n", 0.0, get_frame_pts(context, start), i);
        }
    }
    CLOSEOUTFILE(context.state.ffsplit_file);

    if (context.state.vcf_file.get() && prev < start && start - prev > 5 && prev > 0 )
    {
        fprintf(context.state.vcf_file.get(), "VirtualDub.subset.AddRange(%li,%li);\n", F2F(prev-1), F2F(start) - F2F(prev));
    }
    CLOSEOUTFILE(context.state.vcf_file);

    if (context.state.vdr_file.get() && prev < start && end - start > 2)
    {
        if (start < 5)
            start = 0;
        fprintf(context.state.vdr_file.get(), "%s start\n",	dblSecondsToStrMinutesFrames(context, get_frame_pts(context, start)));
        fprintf(context.state.vdr_file.get(), "%s end\n", dblSecondsToStrMinutesFrames(context, get_frame_pts(context, end)));
    }
    CLOSEOUTFILE(context.state.vdr_file);

    if (context.state.projectx_file.get() && prev < start)
    {
        fprintf(context.state.projectx_file.get(), "%ld\n", F2F(prev+1));
        fprintf(context.state.projectx_file.get(), "%ld\n", F2F(start));
    }
    CLOSEOUTFILE(context.state.projectx_file);

    if (context.state.avisynth_file.get() && prev < start)
    {
        fprintf(context.state.avisynth_file.get(), "%strim(%ld,", (prev < 10 ? "" : " ++ "), F2F(prev+1));
        fprintf(context.state.avisynth_file.get(), "%ld)", F2F(start));
    }
    if (context.state.avisynth_file.get() && last)
    {
        fprintf(context.state.avisynth_file.get(), "\n");
    }
    CLOSEOUTFILE(context.state.avisynth_file);

    if (context.state.videoredo_file.get() && prev < start && end - start > 2)
    {
        if (i == 0 && context.state.demux_pid)
            fprintf(context.state.videoredo_file.get(), "<VideoStreamPID>%d\n<AudioStreamPID>%d\n<SubtitlePID1>%d\n", context.state.selected_video_pid, context.state.selected_audio_pid, context.state.selected_subtitle_pid);
        s_start = max(start-context.settings.videoredo_offset-1,0);
        s_end = max(end - context.settings.videoredo_offset-1,0);
        fprintf(context.state.videoredo_file.get(), "<Cut>%.0f:%.0f\n", get_frame_pts(context, s_start) * 10000000, get_frame_pts(context, s_end) * 10000000);
    }
    CLOSEOUTFILE(context.state.videoredo_file);

    if (context.state.videoredo3_file.get() && prev < start && end - start > 2)
    {
        /*
              <cut Sequence="2" CutStart="00:00:05;10" CutEnd="00:00:20;16" Elapsed="00:00:02;01"> <CutTimeStart>54000113</CutTimeStart> <CutTimeEnd>206400112</CutTimeEnd> </cut>
          */
        if (i == 0 && context.state.demux_pid)
            fprintf(context.state.videoredo3_file.get(), "<InputPIDList><VideoStreamPID>%d</VideoStreamPID>\n<AudioStreamPID>%d</AudioStreamPID><SubtitlePID1>%d</SubtitlePID1></InputPIDList>\n", context.state.selected_video_pid, context.state.selected_audio_pid, context.state.selected_subtitle_pid);
        s_start = max(start-context.settings.videoredo_offset-1,0);
        s_end = max(end - context.settings.videoredo_offset-1,0);
        fprintf(context.state.videoredo3_file.get(), "<Cut><CutTimeStart>%.0f</CutTimeStart> <CutTimeEnd>%.0f</CutTimeEnd> </Cut>\n", get_frame_pts(context, s_start) * 10000000, get_frame_pts(context, s_end) * 10000000);

    }
    if (context.state.videoredo3_file.get())
    {
        if (last)
        {
//            fprintf(videoredo3_file, "</cutlist></VideoReDoProject>\n");
            fprintf(context.state.videoredo3_file.get(), "</CutList>\n");
        }
    }
    CLOSEOUTFILE(context.state.videoredo3_file);

    if (context.state.btv_file.get() && prev < start)
    {
        strcpy(scomment, dblSecondsToStrMinutes(context, get_frame_pts(context, start)));
        strcpy(ecomment, dblSecondsToStrMinutes(context, get_frame_pts(context, end)));

        fprintf(context.state.btv_file.get(), "<Region><start comment=\"%s\">%.0f</start><end comment=\"%s\">%.0f</end></Region>\n",
                scomment, get_frame_pts(context, start) * 10000000, ecomment, get_frame_pts(context, end) * 10000000);
        if (last)
        {
            fprintf(context.state.btv_file.get(), "</cutlist>\n");
        }
    }
    CLOSEOUTFILE(context.state.btv_file);

    if (context.state.edl_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        append_edl_record(context, context.state.edl_file.get(), start, end, comskip::output::EdlVariant::standard);
    }
    CLOSEOUTFILE(context.state.edl_file);

    if (context.state.live_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        append_edl_record(context, context.state.live_file.get(), start, end, comskip::output::EdlVariant::standard);
    }
    CLOSEOUTFILE(context.state.live_file);

    if (context.state.ipodchap_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
//		fprintf(ipodchap_file,"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
        fprintf(context.state.ipodchap_file.get(), "CHAPTER%.2i=%s\nCHAPTER%.2iNAME=%d\n", i+2,dblSecondsToStrMinutes(context, get_frame_pts(context, end)), i+2, i+2 );
    }
    CLOSEOUTFILE(context.state.ipodchap_file);

    if (context.state.edlp_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        if (start < 5)
            start = 0;
        append_edl_record(context, context.state.edlp_file.get(), start, end, comskip::output::EdlVariant::plus);
    }
    CLOSEOUTFILE(context.state.edlp_file);

    if (context.state.bcf_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        fprintf(context.state.bcf_file.get(), "1,%.0f,%.0f\n", get_frame_pts(context, start) * 1000.0, get_frame_pts(context, end) * 1000.0);
    }
    CLOSEOUTFILE(context.state.bcf_file);

    if (context.state.edlx_file.get() && !context.state.frame.empty())
    {
        if (prev < start /* &&!last */ && end - start > 2)
        {
            fprintf(context.state.edlx_file.get(), "<region start=\"%" PRId64 "\" end=\"%" PRId64 "\"/> \n", context.state.frame[start].goppos, context.state.frame[end].goppos);
        }
        if (last)
        {
            fprintf(context.state.edlx_file.get(), "</regionlist>\n");
        }
    }
    CLOSEOUTFILE(context.state.edlx_file);

    if (context.state.womble_file.get())
    {
// CLIPLIST: #1 show
// CLIP: morse.mpg
// 6 0 9963
        if (!last)
        {
            if (start - prev > context.settings.fps)
            {
                fprintf(context.state.womble_file.get(), "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename,F2F(prev+1), F2F(start) - F2F(prev));
            }
// CLIPLIST: #2 commercial
// CLIP: morse.mpg
// 6 9963 5196

            fprintf(context.state.womble_file.get(), "CLIPLIST: #%i commercial\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename, F2F(start), F2F(end) - F2F(start));
        }
        else
        {
            if (end - prev > 0)
                fprintf(context.state.womble_file.get(), "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename, F2F(prev+1), F2F(end) - F2F(prev));
        }
    }
    CLOSEOUTFILE(context.state.womble_file);

    if (context.state.mls_file.get())
    {
        if (i == 0)
        {
            count = (context.state.commercial_count+1)*2+1;
//            if (commercial[commercial_count].end_frame < frame_count-2)
//                count += 2;
            if (start < context.settings.fps)
                count -= 1;
            fprintf(context.state.mls_file.get(), "[BookmarkList]\nPathName= %s\nVideoStreamID= 0\nFormat= frame\nCount= %d\n", context.state.mpegfilename, count);
            if (start >= context.settings.fps)
                fprintf(context.state.mls_file.get(), "%11i 1\n", 0);
        }
        else
            fprintf(context.state.mls_file.get(), "%11li 1\n", F2F(prev));
        if (!last)
            fprintf(context.state.mls_file.get(), "%11li 0\n", F2F(start));
        else if (start < end - 5) {
            fprintf(context.state.mls_file.get(), "%11li 0\n", F2F(start));
            fprintf(context.state.mls_file.get(), "%11li 1\n", F2F(end));
        }

    }
    CLOSEOUTFILE(context.state.mls_file);

    if (context.state.mpgtx_file.get())
    {
        if (!last)
        {
            if (start - prev > 0)
            {
                fprintf(context.state.mpgtx_file.get(), "[%s-",	(prev < context.settings.fps ? "":intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev))));
                fprintf(context.state.mpgtx_file.get(), "%s] ", intSecondsToStrMinutes(context,  (int)get_frame_pts(context, start)));
            }
        }
        else
        {
            if (end - prev > 0)
                fprintf(context.state.mpgtx_file.get(), "[%s-]",	intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev+1)));
            fprintf(context.state.mpgtx_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.mpgtx_file);

    if (context.state.dvrcut_file.get())
    {
        if (start - prev > (int)context.settings.fps /* && start > 2*fps */)
        {
            fprintf(context.state.dvrcut_file.get(), "%s ",	intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev)));
            fprintf(context.state.dvrcut_file.get(), "%s ", intSecondsToStrMinutes(context,  (int)get_frame_pts(context, start)));
        }
        if (last)
        {
            fprintf(context.state.dvrcut_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.dvrcut_file);

    if (context.state.dvrmstb_file.get())
    {
        if (end - start > 1)
        {
            if (start == 1) start = 0;
            fprintf(context.state.dvrmstb_file.get(), "  <commercial start=\"%f\" end=\"%f\" />\n", get_frame_pts(context, start), get_frame_pts(context, end));
        }
        if (last)
        {
            fprintf(context.state.dvrmstb_file.get(), " </root>\n");
        }
    }
    CLOSEOUTFILE(context.state.dvrmstb_file);

    if (context.state.mpeg2schnitt_file.get())
    {
        if (end - start > 1)
        {
            fprintf(context.state.mpeg2schnitt_file.get(), "/o%ld ",	F2F(start));
            fprintf(context.state.mpeg2schnitt_file.get(), "/i%ld ", F2F(end));
        }
        if (last)
        {
            fprintf(context.state.mpeg2schnitt_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.mpeg2schnitt_file);

    if (context.state.cuttermaran_file.get())
    {
        if (prev+1 < start)
        {
            fprintf(context.state.cuttermaran_file.get(), "<CutElements refVideoFile=\"0\" StartPosition=\"%li\" EndPosition=\"%li\">\n", F2F(prev+1), F2F(start-1));
            fprintf(context.state.cuttermaran_file.get(), "<CurrentFiles refVideoFiles=\"0\" /> <cutAudioFiles refAudioFile=\"1\" /></CutElements>\n");
        }
        if (last)
        {
            if (context.settings.cuttermaran_options.c_str()[0] == 0)
                fprintf(context.state.cuttermaran_file.get(), "<CmdArgs OutFile=\"%s_clean.m2v\" cut=\"true\" unattended=\"true\" snapToCutPoints=\"true\" closeApp=\"true\" />\n</StateData>\n",context.state.inbasename);
            else
                fprintf(context.state.cuttermaran_file.get(), "<CmdArgs OutFile=\"%s_clean.m2v\" %s />\n</StateData>\n",context.state.inbasename, context.settings.cuttermaran_options.c_str());
        }
    }
    CLOSEOUTFILE(context.state.cuttermaran_file);
}


char CompareLetter(RecordingContext& context, int value, int average, int i)
{
    if (context.state.cblock[i].reffer == '+' || context.state.cblock[i].reffer == '-')
    {
        if (value > 1.2 * average)
        {
            if (context.state.cblock[i].reffer == '-')
                return('=');
            else
                return('!');
        }
        if (value < 0.8 * average)
        {
            if (context.state.cblock[i].reffer == '-')
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

void BuildCommercial(RecordingContext& context)
{
    int i;
    context.state.commercial_count = -1;
    i = 0;
    while (i < context.state.block_count)
    {
        if (context.state.cblock[i].score > context.settings.global_threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > min_show_segment_length) ))
           )
        {
            context.state.commercial_count++;
            context.state.commercial[context.state.commercial_count].start_frame = context.state.cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
            context.state.commercial[context.state.commercial_count].start_block = i;
            context.state.commercial[context.state.commercial_count].end_block = i;
            context.state.cblock[i].iscommercial = true;
            i++;
            while (i < context.state.block_count && context.state.cblock[i].score > context.settings.global_threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
                  )
            {
                context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
                context.state.commercial[context.state.commercial_count].end_block = i;
                context.state.cblock[i].iscommercial = true;
                i++;
            }
        }
        else
            context.state.cblock[i].iscommercial = false;
        i++;
    }
}


bool OutputBlocks(RecordingContext& context)
{
    int		i,k;
    long	prev;
    double comlength;
    double	threshold;
    bool	foundCommercials = false;
    bool	deleted = false;

    if (context.settings.global_threshold >= 0.0)
    {
        threshold = context.settings.global_threshold;
    }
    else
    {
        threshold = FindScoreThreshold(context, context.settings.score_percentile);
    }

    OpenOutputFiles(context);


    Debug(context, 1, "Threshold used - %.4f", threshold);
    threshold = ceil(threshold * 100) / 100.0;
    Debug(context, 1, "\tAfter rounding - %.4f\n", threshold);

    BuildCommercial(context);

#ifdef undef
    context.state.commercial_count = -1;
    i = 0;
    while (i < context.state.block_count)
    {
        if (context.state.cblock[i].score > threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
           )
        {
            context.state.commercial_count++;
            context.state.commercial[context.state.commercial_count].start_frame = context.state.cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
            context.state.commercial[context.state.commercial_count].start_block = i;
            context.state.commercial[context.state.commercial_count].end_block = i;
            context.state.cblock[i].iscommercial = true;
            i++;
            while (i < context.state.block_count && context.state.cblock[i].score > threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) >  (min_show_segment_length) ))
                  )
            {
                context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
                context.state.commercial[context.state.commercial_count].end_block = i;
                context.state.cblock[i].iscommercial = true;
                i++;
            }
        }
        else
            context.state.cblock[i].iscommercial = false;
        i++;
    }
#endif


    if (!(context.settings.disable_heuristics & (1 << (5 - 1))))
    {

        if (context.settings.delete_block_after_commercial > 0)
        {
            for (k = context.state.commercial_count; k >= 0; k--)
            {
                i = context.state.commercial[k].end_block + 1;
                if (i < context.state.block_count && context.state.cblock[i].length < context.settings.delete_block_after_commercial &&
                        context.state.cblock[i].score < threshold)
                {
                    Debug(context, 3, "H5 Deleting cblock %i because it is short and comes after a commercial.\n",
                          i);
                    context.state.commercial[k].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                    context.state.commercial[k].length = F2L(context.state.commercial[k].end_frame, context.state.commercial[k].start_frame);
                    context.state.commercial[k].end_block = i;
                    context.state.cblock[i].iscommercial = true;
                    context.state.cblock[i].cause |= C_H5;
                    context.state.cblock[i].score = 99.99;
                    context.state.cblock[i].more |= C_H5;
                }
            }
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[context.state.commercial_count].end_block < context.state.block_count - 1 &&
                F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].end_block].f_end) < context.settings.min_show_segment_length / 2.0 )
        {
            context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial and its too short.\n",
                  context.state.block_count-1, (int)context.state.cblock[context.state.block_count-1].length);
            context.state.cblock[context.state.block_count-1].cause |= C_H5;
            context.state.cblock[context.state.block_count-1].score = 99.99;
            context.state.cblock[context.state.block_count-1].more |= C_H5;
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[0].start_block == 1 &&
                F2T(context.state.cblock[0].f_end) < context.settings.min_commercialbreak)
        {
            context.state.commercial[0].start_block = 0;
            context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[0].length = F2L(context.state.commercial[0].end_frame,	context.state.commercial[0].start_frame);
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because its too short and before first commercial.\n",
                  0, (int)context.state.cblock[0].length);
            context.state.cblock[0].score = 99.99;
            context.state.cblock[0].cause |= C_H5;
            context.state.cblock[0].more |= C_H5;

        }

    }


    Debug(context, 2, "\n\n\t---------------------\n\tInitial Commercial List\n\t---------------------\n");
    for (i = 0; i <= context.state.commercial_count; i++)
    {
        Debug(context,
            2,
            "%2i) %6i\t%6i\t%s\n",
            i,
            context.state.commercial[i].start_frame,
            context.state.commercial[i].end_frame,
            dblSecondsToStrMinutes(context, context.state.commercial[i].length)
        );
    }

#if 1




    if (!(context.settings.disable_heuristics & (1 << (6 - 1))))
    {

        // Delete too long/short commercials
        for (k = context.state.commercial_count; k >= 0; k--)
        {
            if ( (F2T(context.state.commercial[k].start_frame) > 1.0   || context.state.commercial[k].length < 10.2 /* Sage bug fix */ )
                    &&		// Do not delete too short first or last commercial
                    ((context.state.commercial[k].length > context.settings.max_commercialbreak && k != 0 && k != context.state.commercial_count) ||
                     (context.state.commercial[k].length < context.settings.min_commercialbreak)) &&
                    F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) > context.settings.min_commercial_break_at_start_or_end  &&
                    F2T(context.state.commercial[k].end_frame) > context.settings.min_commercial_break_at_start_or_end )
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(context, 3, "H6 Deleting block %i because it is part of a too short or too long commercial.\n",
                          i);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
                }
                for (i = k; i < context.state.commercial_count; i++)
                {
                    context.state.commercial[i] = context.state.commercial[i + 1];
                }
                context.state.commercial_count--;
                deleted = true;
            }
        }
#ifdef NOTDEF
// keep first seconds
        if (always_keep_first_seconds && context.state.commercial_count >= 0)
        {
            k = 0;
            if ( F2T(context.state.commercial[k].end_frame) < always_keep_first_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the first %d seconds should always be kept.\n",
                          i, always_keep_first_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
                }
                for (i = k; i < context.state.commercial_count; i++)
                {
                    context.state.commercial[i] = context.state.commercial[i + 1];
                }
                context.state.commercial_count--;
                deleted = true;
            }
        }
        if (always_keep_last_seconds && context.state.commercial_count >= 0)
        {
            k = context.state.commercial_count;
            if (F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < always_keep_last_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the last %d seconds should always be kept.\n",
                          i, always_keep_last_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
                }
                for (i = k; i < context.state.commercial_count; i++)
                {
                    context.state.commercial[i] = context.state.commercial[i + 1];
                }
                context.state.commercial_count--;
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
    if (context.settings.delete_show_after_last_commercial &&
            context.state.commercial_count > -1 &&
            //	( commercial[commercial_count].end_block == block_count - 2 || commercial[commercial_count].end_block == block_count - 3) &&
            ((context.settings.delete_show_after_last_commercial == 1 && context.state.cblock[context.state.commercial[context.state.commercial_count].start_block].f_end > context.state.before_end) ||
             (context.settings.delete_show_after_last_commercial > F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].start_block].f_start)) )

            &&
            context.state.commercial[context.state.commercial_count].end_block < context.state.block_count-1
       )
    {
        i = context.state.commercial[context.state.commercial_count].end_block + 1;
        context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
        context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
        while (i < context.state.block_count)
        {
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial.\n",
                  i, (int)context.state.cblock[i].length );
            context.state.cblock[i].cause |= C_H5;
            context.state.cblock[i].score = 99.99;
            context.state.cblock[i].more |= C_H5;
            i++;
        }
    }



    if (context.settings.delete_show_before_first_commercial &&
            context.state.commercial_count > -1 &&
            context.state.commercial[0].start_block == 1 &&
            ((context.settings.delete_show_before_first_commercial == 1 && context.state.cblock[context.state.commercial[0].end_block].f_end < context.state.after_start) ||
             (context.settings.delete_show_before_first_commercial > F2T(context.state.cblock[context.state.commercial[0].end_block].f_end)))
       )
    {
        context.state.commercial[0].start_block = 0;
        context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[0].length = F2L(context.state.commercial[0].end_frame, context.state.commercial[0].start_frame);
        Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes before the first commercial.\n",
              0, (int)context.state.cblock[0].length);
        context.state.cblock[0].score = 99.99;
        context.state.cblock[0].cause |= C_H5;
        context.state.cblock[0].more |= C_H5;

    }

// keep first seconds
    if (context.settings.always_keep_first_seconds && context.state.commercial_count >= 0)
    {
        k = 0;
        while (context.state.commercial_count >= 0 && F2T(context.state.commercial[k].end_frame) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, "Deleting commercial block %i because the first %d seconds should always be kept.\n",
                  k, context.settings.always_keep_first_seconds);
            for (i = k; i <= context.state.commercial_count; i++)
            {
                context.state.commercial[i] = context.state.commercial[i + 1];
            }
            context.state.commercial_count--;
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && F2T(context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, "Shortening commercial block %i because the first %d seconds should always be kept.\n",
                  k, context.settings.always_keep_first_seconds);
            while (F2T(context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds && context.state.commercial[k].start_frame < context.settings.always_keep_first_seconds * context.settings.fps)
                context.state.commercial[k].start_frame++;
        }
    }
    if (context.settings.always_keep_last_seconds && context.state.commercial_count >= 0)
    {
        k = context.state.commercial_count;
        while (context.state.commercial_count >= 0 && F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, "Deleting commercial block %i because the last %d seconds should always be kept.\n",
                  k, context.settings.always_keep_last_seconds);
            context.state.commercial_count--;
            k = context.state.commercial_count;
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, "Shortening commercial block %i because the last %d seconds should always be kept.\n",
                  k, context.settings.always_keep_last_seconds);
            while (F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds && (context.state.cblock[context.state.block_count-1].f_end - context.state.commercial[k].end_frame) < context.settings.fps * context.settings.always_keep_last_seconds)
                context.state.commercial[k].end_frame--;
        }
    }



    if (deleted)
        Debug(context, 1, "\n\n\t---------------------\n\tFinal Commercial List\n\t---------------------\n");
    else
        Debug(context, 1, "No change\n");
#endif


    // Apply padding
    for (i = 0; i <= context.state.commercial_count; i++)
    {
        context.state.commercial[i].start_frame += context.settings.padding*context.settings.fps - context.settings.remove_before*context.settings.fps;
        context.state.commercial[i].end_frame -= context.settings.padding*context.settings.fps - context.settings.remove_after*context.settings.fps;
        if (context.state.commercial[i].end_frame > context.state.frame_count)
            context.state.commercial[i].end_frame = context.state.frame_count;
        context.state.commercial[i].length += -2*context.settings.padding + context.settings.remove_before + context.settings.remove_after;
    }



    comlength = 0.;
    for (i = 0; i < context.state.commercial_count; i++)
    {
        comlength += context.state.commercial[i].length;
    }
//	Debug(1, "Total commercial length found: %s\n",	dblSecondsToStrMinutes(comlength));

    if ((context.state.zoomplayer_chapter_file.get()) &&
//		(commercial[0].length >= min_commercialbreak) &&
//		(commercial[0].length <= max_commercialbreak) &&
            (context.state.commercial[0].start_frame > 5))
    {
        fprintf(context.state.zoomplayer_chapter_file.get(), "AddChapter(1,Show Segment)\n");
    }

    if (context.state.ffmeta_file.get()) {
        fprintf(context.state.ffmeta_file.get(), ";FFMETADATA1\n");
    }

    prev = -1;
    for (i = 0; i <= context.state.commercial_count; i++)
    {
//		if ((commercial[i].length >= min_commercialbreak) && (commercial[i].length <= max_commercialbreak))
        {
            foundCommercials = true;
            if (deleted)
                Debug(context,
                    1,
                    "%i - start: %6i\tend: %6i\t[%6i:%6i]\tlength: %s\n",
                    i + 1,
                    context.state.commercial[i].start_frame,
                    context.state.commercial[i].end_frame,
                    context.state.commercial[i].start_block,
                    context.state.commercial[i].end_block,
                    dblSecondsToStrMinutes(context, context.state.commercial[i].length)
                );
            OutputCommercialBlock(context, i, prev, context.state.commercial[i].start_frame, context.state.commercial[i].end_frame, (context.state.commercial[i].end_frame < context.state.frame_count-2 ? false : true));
            prev = context.state.commercial[i].end_frame;
        }
    }

    if (context.state.commercial[context.state.commercial_count].end_frame < context.state.frame_count-2)
        OutputCommercialBlock(context, context.state.commercial_count+1, prev, context.state.frame_count-2, context.state.frame_count-1, true);

    if (context.settings.output_videoredo)
    {
        comskip::checked_format(context.state.filename, "%s.VPrj", context.state.outbasename);
        context.state.videoredo_file.reset(myfopen(context.state.filename, "a+"));
        if (context.state.videoredo_file.get())
        {
            for (i = 0; i < context.state.block_count; i++)
            {
                fprintf(context.state.videoredo_file.get(), "<SceneMarker %d>%.0f\n", i, F2T(max(context.state.cblock[i].f_end-context.settings.videoredo_offset-1,0)) * 10000000);
            }
            context.state.videoredo_file.reset();
        }
    }

    if (context.settings.output_videoredo3)
    {
        comskip::checked_format(context.state.filename, "%s.VPrj", context.state.outbasename);
        context.state.videoredo3_file.reset(myfopen(context.state.filename, "a+"));
        if (context.state.videoredo3_file.get())
        {
            fprintf(context.state.videoredo3_file.get(), "<SceneList>\n");
            for (i = 0; i < context.state.block_count; i++)
            {
// <SceneList>
//   <SceneMarker Sequence="1" Timecode="00:00:56;00">560560112</SceneMarker>
// </SceneList>
                   fprintf(context.state.videoredo3_file.get(), "<SceneMarker Sequence=\"%d\" Timecode=\"%s\">%.0f</SceneMarker>\n", i, dblSecondsToStrMinutes(context, F2T(max(context.state.cblock[i].f_end-context.settings.videoredo_offset-1,0))) , F2T(max(context.state.cblock[i].f_end-context.settings.videoredo_offset-1,0)) * 10000000);
            }
            fprintf(context.state.videoredo3_file.get(), "</SceneList>\n");
            fprintf(context.state.videoredo3_file.get(), "</VideoReDoProject>\n");
            context.state.videoredo3_file.reset();
        }
    }

    if (context.settings.output_chapters)
    {
//		comskip::checked_format(filename, "%s.chap", outbasename);
//		chapters_file = myfopen(filename, "a+");
        if (context.state.chapters_file.get())
        {
            for (i = 0; i < context.state.block_count; i++)
            {
                fprintf(context.state.chapters_file.get(), "%ld\n", context.state.cblock[i].f_end);
            }
            context.state.chapters_file.reset();
        }
    }

    if (context.state.mkvtoolnix_chapters_file.get())
    {
        double currentStart = 0;
        char startTimespan[15];
        char endTimespan[15];

        if(context.settings.output_mkvtoolnix > 0){
            fprintf(context.state.mkvtoolnix_chapters_file.get(),"\t<EditionEntry>\n\t\t<EditionUID>1</EditionUID>\n");
            for (i = 0; i < context.state.block_count; i++)
            {
                if(i == 0 || (context.state.cblock[i-1].iscommercial != context.state.cblock[i].iscommercial)){
                        currentStart = context.state.cblock[i].f_start;
                }
                if(i == i-1 || (context.state.cblock[i+1].iscommercial != context.state.cblock[i].iscommercial)){
                    strcpy(startTimespan, dblSecondsToStrMinutes(context, get_frame_pts(context, currentStart)));
                    fprintf(context.state.mkvtoolnix_chapters_file.get(),
                        "\t\t<ChapterAtom>\n"\
                        "\t\t\t<ChapterDisplay>\n"\
                        "\t\t\t\t<ChapterString>%s</ChapterString>\n"\
                        "\t\t\t</ChapterDisplay>\n"\
                        "\t\t\t<ChapterTimeStart>%s</ChapterTimeStart>\n"\
                        "\t\t</ChapterAtom>\n"
                    , context.state.cblock[i].iscommercial ? "Commercial" : "Show", startTimespan);
                }
            }
            fprintf(context.state.mkvtoolnix_chapters_file.get(),"\t</EditionEntry>\n");
        }
        if(context.settings.output_mkvtoolnix == 2){
            fprintf(context.state.mkvtoolnix_chapters_file.get(),"\t<EditionEntry>\n\t\t<EditionUID>2</EditionUID>\n\t\t<EditionFlagOrdered>1</EditionFlagOrdered>\n");
            for (i = 0; i < context.state.block_count; i++)
            {
                if(!context.state.cblock[i].iscommercial){
                    if(i == 0 || context.state.cblock[i-1].iscommercial){
                        currentStart = context.state.cblock[i].f_start;
                    }
                    if(i == i-1 || context.state.cblock[i+1].iscommercial){
                        strcpy(startTimespan, dblSecondsToStrMinutes(context, get_frame_pts(context, currentStart)));
                        strcpy(endTimespan, dblSecondsToStrMinutes(context, get_frame_pts(context, context.state.cblock[i].f_end)));
                        fprintf(context.state.mkvtoolnix_chapters_file.get(),
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
            fprintf(context.state.mkvtoolnix_chapters_file.get(),"\t</EditionEntry>\n");
        }
        fprintf(context.state.mkvtoolnix_chapters_file.get(),"</Chapters>");
        context.state.mkvtoolnix_chapters_file.reset();
    }

    if (context.state.reffer_count == -1) {
        context.state.reffer_count = context.state.commercial_count;
        for (i = 0; i <= context.state.commercial_count; i++)
        {
            context.state.reffer[i].start_frame = context.state.commercial[i].start_frame;
            context.state.reffer[i].end_frame = context.state.commercial[i].end_frame;
        }
    }

    InputReffer(context, ".ref", false);

    if (context.settings.output_tuning)
    {
        comskip::checked_format(context.state.filename, "%s.tun", context.state.workbasename);
        context.state.tuning_file.reset(myfopen(context.state.filename, "w"));
        fprintf(context.state.tuning_file.get(),"max_volume=%6i\n", context.state.min_volume+200);
        fprintf(context.state.tuning_file.get(),"max_avg_brightness=%6i\n", context.state.min_brightness_found+5);
        fprintf(context.state.tuning_file.get(),"max_commercialbreak=%6i\n", context.state.max_logo_gap+10);
        fprintf(context.state.tuning_file.get(),"shrink_logo=%.2f\n", context.state.logo_overshoot);
        fprintf(context.state.tuning_file.get(),"min_show_segment_length=%6i\n", context.state.max_nonlogo_block_length+10);
        fprintf(context.state.tuning_file.get(),"logo_threshold=%.3f\n", context.state.logo_quality);
    }




    if (context.settings.verbose)
    {
        Debug(context, 1, "\nLogo fraction:              %.4f      %s\n",context.state.logoPercentage, ((context.settings.commDetectMethod & LOGO) ? (context.state.reverseLogoLogic? "(Reversed Logo Logic)": "") : "Logo disabled") );
        Debug(context, 1,   "Maximum volume found:       %6i\n", context.state.maxi_volume);
        Debug(context, 1,   "Average volume:             %6i\n", context.state.avg_volume);
        Debug(context, 1,   "Sound threshold:            %6i\n", context.settings.max_volume);
        Debug(context, 1,   "Silence threshold:          %6i\n", context.settings.max_silence);
        Debug(context, 1,   "Minimum volume found:       %6i\n", context.state.min_volume);
        Debug(context, 1,   "Average frames with silence:%6i\n", context.state.avg_silence);
        Debug(context, 1,   "Black threshold:            %6i\n", context.settings.max_avg_brightness);
        Debug(context, 1,   "Minimum brightness found:   %6i\n", context.state.min_brightness_found);
        Debug(context, 1,   "Minimum bright pixels found:%6i\n", context.state.min_hasBright);
        Debug(context, 1,   "Minimum dim level found:    %6i\n", context.state.min_dimCount);
        Debug(context, 1,   "Average brightness:         %6i\n", context.state.avg_brightness);
        Debug(context, 1,   "Uniformity level:           %6i\n", context.settings.non_uniformity);
        Debug(context, 1,   "Average non uniformity:     %6i\n", context.state.avg_uniform);
        Debug(context, 1,   "Maximum gap between logo's: %6i\n", context.state.max_logo_gap);
        Debug(context, 1,   "Suggested logo_threshold:   %.4f\n",context.state.logo_quality);
        Debug(context, 1,   "Suggested shrink_logo:	    %.2f\n", context.state.logo_overshoot);
        Debug(context, 1,   "Max commercial size found:  %6i\n", context.state.max_nonlogo_block_length);
        Debug(context, 1,   "Dominant aspect ratio:      %.4f\n",context.state.dominant_ar);
        Debug(context, 1,   "Score threshold:            %.4f\n", threshold);
        Debug(context, 1,   "Framerate:                  %2.3f\n", context.settings.fps);
        Debug(context, 1,   "Average framerate:          %2.3f\n", context.state.avg_fps);

        Debug(context, 1,   "Total commercial length:    %s\n",	dblSecondsToStrMinutes(context, comlength));
        Debug(context, 1,   "Cut codes:\n");
        Debug(context, 1,   "  F: scene\t c: change\n  A: aspect\t t: cutscene\n  E: exceeds\t l: logo\n  L: logo\t v: volume\n  B: bright\t s: scene_change\n  C: combined\t a: aspect_ratio\n  N: nonstrict\t u: uniform_frame\n  S: strict\t b: black_frame\n  \t\t r: resolution\n");
        Debug(context, 1,   "----------------------------------------------------\n");
        Debug(context, 1,   "Block list after weighing\n----------------------------------------------------\n", threshold);
        Debug(context,
            1,
            "  #     sbf  bs  be     fs     fe        ts        te       len     sc   scr cmb   ar                   cut    bri logo   vol sil   corr stdev   cc\n"
        );

//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 0, 0,0,0, 0, 0, 0, 0);
//		}




        for (i = 0; i < context.state.block_count; i++)
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

            Debug(context,
                1,
                "%3i:%c%c %4i %3i %3i %6i %6i %8.2fs %8.2fs %8.2fs %6.2f %5.2f %3i %4.2f %s %4i%c %4.2f %4i%c %2i%c %6.3f %5i %-10s",
                i,
                CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                context.state.cblock[i].bframe_count,
                context.state.cblock[i].b_head,
                context.state.cblock[i].b_tail,
                context.state.cblock[i].f_start,
                context.state.cblock[i].f_end,
                get_frame_pts(context, context.state.cblock[i].f_start),
                get_frame_pts(context, context.state.cblock[i].f_end),
                context.state.cblock[i].length,
                context.state.cblock[i].score,
//				cblock[i].schange_count,
                context.state.cblock[i].schange_rate,
                context.state.cblock[i].combined_count,
                context.state.cblock[i].ar_ratio,
                CauseString(context, context.state.cblock[i].cause),
                context.state.cblock[i].brightness,
                CompareLetter(context, context.state.cblock[i].brightness,context.state.avg_brightness,i),
                context.state.cblock[i].logo,
                context.state.cblock[i].volume,
                CompareLetter(context, context.state.cblock[i].volume,context.state.avg_volume,i),
                context.state.cblock[i].silence,
                CompareLetter(context, context.state.cblock[i].silence,context.state.avg_silence,i),
                0.0 /*cblock[i].correlation */ ,
                context.state.cblock[i].stdev,
                CCTypeToStr(context, context.state.cblock[i].cc_type)
            );
            if (context.settings.commDetectMethod & LOGO)
            {
//				if (CheckFramesForLogo(cblock[i].f_start, cblock[i].f_end)) {
//					Debug(1, "\tLogo Present\n");
//				} else {
                Debug(context, 1, "\n");
//				}
            }
            else
            {
                Debug(context, 1, "\n");
            }
        }

        OutputAspect(context);
        OutputTraining(context);



//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 100, 0,0,0, 0, 0, 0, 0);
//		}

    }

//	OutputDebugWindow(false,0);
    return (foundCommercials);
}

void OutputStrict(RecordingContext& context, double len, double delta, double tol)
{
//return;
    if (context.settings.output_training && !context.state.training_file.get())
    {
        context.state.training_file.reset(myfopen("strict.csv", "a+"));
//		fprintf(training_file, "// score, length, fraction, position,combined, ar error, logo, strict \n");
    }
    if (context.state.training_file.get())
        fprintf(context.state.training_file.get(), "%+f,%+f,%+f, %s\n", len,delta, tol, context.state.inbasename);
}




void OutputTraining(RecordingContext& context)
{
    int i;
//	return;
    if (!context.settings.output_training)
        return;
    context.state.training_file.reset(myfopen("comskip.csv", "a+"));

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


    r = (context.state.commercial[0].start_frame/fps < 30.0 ? context.state.commercial_count: context.state.commercial_count+1);
    if (context.state.commercial[0].start_frame/fps < 30.0)
        s = context.state.commercial[0].end_frame;
    else
        s = 0;
    fprintf(training_file, "\"%s\",%f,%d,", inbasename,  (context.state.commercial[context.state.commercial_count].start_frame - s)/fps, r);
    for (i = 0; i < 40; i++)
    {
        if (i <= context.state.commercial_count)
        {
            if (i == 0)
                e = 0;
            else
                e = context.state.commercial[i-1].end_frame;
            if (i == context.state.commercial_count)
                s = 0;
            else
                s = (context.state.commercial[i].end_frame - context.state.commercial[i].start_frame);
            if (i > 0)
                fprintf(training_file, "%f,%f,", (context.state.commercial[i].start_frame-e)/fps, s/fps);
            else
            {
                if (context.state.commercial[i].start_frame/fps > 30.0)
                    fprintf(training_file, "%f,%f, %f,%f,", 0.0, 0.0, (context.state.commercial[i].start_frame-e)/fps,s/fps);
                else
                    fprintf(training_file, "%f,%f,", (context.state.commercial[i].start_frame-e)/fps,s/fps);
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

    fprintf(context.state.training_file.get(), "block, cm,rf, score, length, start, end, fromend ar, logo, cause, less, more\n");

    for (i = 0; i < context.state.block_count; i++)
    {
        if (context.settings.output_training)
        {
            fprintf(context.state.training_file.get(), TRAINING_LAYOUT,
                    i,
                    CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    context.state.cblock[i].score,
                    context.state.cblock[i].length,
                    F2T(context.state.cblock[i].f_start),
                    F2T(context.state.cblock[i].f_end),
                    F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[i].f_end),
                    context.state.cblock[i].ar_ratio,
                    context.state.cblock[i].logo,
                    CauseString(context, context.state.cblock[i].cause),
                    CauseString(context, context.state.cblock[i].less),
                    CauseString(context, context.state.cblock[i].more),
                    context.state.inbasename);

        }
    }
#endif

}
