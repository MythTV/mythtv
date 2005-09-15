#include <qapplication.h>
#include <qsqldatabase.h>

#include <iostream>
#include <iomanip>
#include <unistd.h>

using namespace std;

#include "avformat.h"
#include "avcodec.h"

#include "mythcontext.h"
#include "programinfo.h"
#include "exitcodes.h"
#include "jobqueue.h"

#include "mpeg2trans.h"

typedef QMap<long long, int> DeleteMap;
typedef DeleteMap::iterator DeleteMapIterator;

typedef QMap<long long, long long> PositionMap;
typedef PositionMap::iterator PositionMapIterator;


//#define MPEG2trans_DEBUG

MPEG2trans::MPEG2trans(ProgramInfo *pginfo, bool use_db)
{
    m_pginfo = pginfo;

    chkTranscodeDB = use_db;

    inputFC = NULL;
    outputFC = NULL;

    // Register all formats and codecs 
    av_register_all();
}

MPEG2trans::~MPEG2trans()
{
}

bool MPEG2trans::InitAV(QString &inputfile)
{
    int ret;

    // Open recording
    ret = av_open_input_file(&inputFC, inputfile.ascii(), NULL, 0, NULL);
    if (ret != 0)
    {
        VERBOSE( VB_IMPORTANT, QString("Couldn't open input file, error #%1").arg(ret));
        return false;
    }

    // Getting stream information
    ret = av_find_stream_info(inputFC);
    if (ret < 0)
    {
        VERBOSE( VB_IMPORTANT, QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return false;
    }

    // Dump stream information
    dump_format(inputFC, 0, inputfile.ascii(), false);

    return true;
}

bool MPEG2trans::InitOutput(QString &outfile)
{
    int ret;

    // Allocate output format context
    outputFC = av_alloc_format_context();
    if (!outputFC)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't allocate output context");
        av_close_input_file(inputFC);
        inputFC = NULL;
        return false;
    }

    // Getting output file format
    AVOutputFormat *fmt = guess_format("dvd", outfile.ascii(), "video/mpeg");
//    AVOutputFormat *fmt = guess_format("vob", outfile.ascii(), "video/mpeg");
    if (!fmt)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't get VOB output context");
        av_close_input_file(inputFC);
        inputFC = NULL;
        return false;
    }

    outputFC->oformat = fmt;

    // Allocate output streams
    for (int i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *ist = inputFC->streams[i];
        AVStream *ost = av_new_stream(outputFC, ist->id);

        ost->stream_copy = 1;

        // copy codec data from input streams
        if (ost->codec && ist->codec)
        {
            ost->codec->codec_id = ist->codec->codec_id;
            ost->codec->codec_type = ist->codec->codec_type;
            
            if (!ost->codec->codec_tag)
                ost->codec->codec_tag = ist->codec->codec_tag;
            
            ost->codec->bit_rate = ist->codec->bit_rate;
            ost->codec->extradata = ist->codec->extradata;
            ost->codec->extradata_size = ist->codec->extradata_size;
            
            switch (ost->codec->codec_type)
            {
                case CODEC_TYPE_AUDIO:
                    ost->codec->sample_rate = ist->codec->sample_rate;
                    ost->codec->channels = ist->codec->channels;
                    ost->codec->frame_size = ist->codec->frame_size;
                    ost->codec->block_align = ist->codec->block_align;
                    break;
                case CODEC_TYPE_VIDEO:
                    ost->codec->time_base = ist->codec->time_base;
                    ost->codec->width = ist->codec->width;
                    ost->codec->height = ist->codec->height;
                    ost->codec->has_b_frames = ist->codec->has_b_frames;
                    break;
                default:
                    VERBOSE(VB_IMPORTANT, QString("Dropping unknown stream type %1.")
                                                 .arg(ost->codec->codec_type));
            }
        }
    }

    strncpy(outputFC->filename, outfile.ascii(), sizeof(outputFC->filename));

    // Dump output format
    dump_format(outputFC, 0, outfile.ascii(), 1);

    // Open output file
    ret = url_fopen(&outputFC->pb, outfile.ascii(), URL_WRONLY);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't open output file, error #%1")
                                      .arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return false;
    }
 
    // Set output parameters
    ret = av_set_parameters(outputFC, NULL);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't set output parameters, error #%1")
                                     .arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return false;
    }

    outputFC->max_delay = 700000;

    return true;
}

AVPacketList *MPEG2trans::ReadGop(int index, int64_t *min_dts, int64_t *min_pts, int64_t *end_pts)
{
    AVPacketList *pkts = NULL;
    AVPacket pkt;
    int iframe_count = 0;

    av_init_packet(&pkt);

    do
    {
        int ret = av_read_frame(inputFC, &pkt);
        if (ret < 0) break;

        if ((inputFC->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_VIDEO) && (pkt.flags & PKT_FLAG_KEY))
            iframe_count++;

        if (iframe_count < 2)
        {
            if (!pkts)
            {
                pkts = new AVPacketList;
                pkts->next = NULL;
                pkts->pkt = pkt;
            }
            else
            {
                AVPacketList *t = pkts;

                while (t->next != NULL) t = t->next;

                t->next = new AVPacketList;
                t->next->next = NULL;
                t->next->pkt = pkt;
            }

            if (pkt.stream_index == index)
            {
                if (pkt.dts != (int64_t)AV_NOPTS_VALUE && min_dts)
                    if ((pkt.dts < *min_dts) || (*min_dts == (int64_t)AV_NOPTS_VALUE)) *min_dts = pkt.dts;
                if (pkt.pts != (int64_t)AV_NOPTS_VALUE && min_pts)
                    if ((pkt.pts < *min_pts) || (*min_pts == (int64_t)AV_NOPTS_VALUE))  *min_pts = pkt.pts;
                if (pkt.pts != (int64_t)AV_NOPTS_VALUE && end_pts)
                    if ((pkt.pts + pkt.duration > *end_pts) || (*end_pts == (int64_t)AV_NOPTS_VALUE)) *end_pts = pkt.pts + pkt.duration;
            }
        }
        else if (pkt.destruct) pkt.destruct(&pkt);

    } while (iframe_count < 2);
    
    return pkts;
}

void MPEG2trans::FreeGop(AVPacketList *pkts)
{
    AVPacketList *t = pkts;
    AVPacketList *u;

    while (t)
    {
        if (t->pkt.destruct) t->pkt.destruct(&t->pkt);

        u = t;
        t = t->next;
        delete u;
    }
}

int MPEG2trans::DoTranscode(QString &infile, QString &tmpfile, bool useCutlist)
{
    DeleteMap deleteMap;
    PositionMap posMap;

    AVPacketList *pkts;
    AVPacket pkt;

    int video_index = -1;

    int64_t start_pts;
    int64_t end_pts;
    int64_t min_dts = AV_NOPTS_VALUE;
    int64_t delta_pts;
    int64_t next_start_pts = AV_NOPTS_VALUE;

    int total_frames = 0;
    int completed_frames = 0;

    int ret;

    int jobID = -1;
    if (chkTranscodeDB)
    {
        jobID = JobQueue::GetJobID(JOB_TRANSCODE, m_pginfo->chanid,
			m_pginfo->recstartts);

        if (jobID < 0)
        {
            VERBOSE(VB_IMPORTANT, "ERROR, Transcode called from JobQueue but "
                                  "no jobID found!");
            return TRANSCODE_EXIT_INVALID_CMDLINE;
        }

        JobQueue::ChangeJobComment(jobID, "0% " + QObject::tr("Completed"));
    }
    

        
    QString clText("will NOT be");
    if (useCutlist)
    {
        clText = "will be";
    }
    if (m_pginfo->subtitle)
    {
        VERBOSE(VB_IMPORTANT, QString("Begining MPEG2 to MPEG2 transcode of ""%1 - %2""... cutlist %3 honored.")
                                      .arg(m_pginfo->title).arg(m_pginfo->subtitle)
                                      .arg(clText));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Begining MPEG2 to MPEG2 transcode of ""%1""... cutlist %3 honored")
                                      .arg(m_pginfo->title).arg(clText));

    }                                      


    m_pginfo->GetPositionMap(posMap, MARK_GOP_BYFRAME);
    if (posMap.empty())
    {
        // no MARK_GOP_BYFRAME entries in the recordedmarkup table
        // trying MARK_GOP_START
        m_pginfo->GetPositionMap(posMap, MARK_GOP_START);
        if (posMap.empty())
        {
            VERBOSE(VB_IMPORTANT, QString("No position map found, this is bad"));
        }
    }

    if (useCutlist)
        m_pginfo->GetCutList(deleteMap);

    // keep frames from beginning of file if
    // delete map hasn't marked them for removal
    PositionMapIterator pMapIter;

    pMapIter = posMap.begin();
    deleteMap.insert(pMapIter.key(), 0, false);

    pMapIter = posMap.end();
    pMapIter--;
    deleteMap.insert(pMapIter.key(), 1, false);
    
    total_frames = pMapIter.key();
    
#ifdef MPEG2trans_DEBUG
    // Iterate over the position map
    for (pMapIter = posMap.begin(); pMapIter != posMap.end(); pMapIter++)
    {
        VERBOSE(VB_FILE, QString("posMap[%1] == %2").arg(pMapIter.key())
                         .arg(pMapIter.data()));
    }
#endif
    
    DeleteMapIterator dMapIter;

//     // Iterate over the cut list
     
//     for (dMapIter = deleteMap.begin(); dMapIter != deleteMap.end(); )
//     {
// #ifdef MPEG2trans_DEBUG
//         cerr << "delMap[" << dMapIter.key() << "] = " << dMapIter.data() << endl;
// #endif
//         if (dMapIter.data() == 0)
//         {
//             start_frame = dMapIter.key();
//             dMapIter++;
//             if (dMapIter != deleteMap.end())
//             {
//                 end_frame = dMapIter.key();
//                 //total_frames += end_frame - start_frame;
//             }
//             else break;
//         }
// 
//         dMapIter++;
//     }

    /*============ initialise AV ===============*/
    if (!InitAV(infile))
        return TRANSCODE_EXIT_UNKNOWN_ERROR;

    if (!InitOutput(tmpfile))
        return TRANSCODE_EXIT_UNKNOWN_ERROR;

    /*=========== transcoding/cuting ===========*/
    av_init_packet(&pkt);

    // initialise cut list iterator
    dMapIter = deleteMap.begin();

    // seek to first entry in deleteMap
    VERBOSE(VB_FILE, QString("First entry in delete map == %1")
                             .arg(dMapIter.key()));
    
    VERBOSE(VB_FILE, QString("Offset in file == %1")
                             .arg(posMap[dMapIter.key()]));

    // write stream header
    ret = av_write_header(outputFC);
    if (ret < 0)
    {
        cerr << "Error writing output header\n";
        return TRANSCODE_EXIT_UNKNOWN_ERROR;
    }
    
    /* find the index of the first video stream */
    for (int i = 0; i < inputFC->nb_streams; i++)
    {
        if (inputFC->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    /* check if no video stream */
    if (video_index == -1)
        return TRANSCODE_BUGGY_EXIT_INVALID_VIDEO;
    
    /* iterate over the entries in the delete map */
    while (dMapIter != deleteMap.end())
    {
        if (dMapIter.data() == 0)   // keep frames
        {
            long long start_key = dMapIter.key();
            while ((posMap.find(start_key) == posMap.end()) && (start_key >= 0)) start_key--;
            if (posMap.find(start_key) == posMap.end())
            {
                VERBOSE(VB_IMPORTANT, "Couldn't find keyframe to start from");
                continue;
            }
            
            dMapIter++;
            if (dMapIter == deleteMap.end())
            {
                VERBOSE(VB_FILE, "Reached end of delete map looking for keyframe.");
                continue;
            }
            
            long long end_key = dMapIter.key();
            while ((posMap.find(end_key) == posMap.end()) && (end_key >= 0)) end_key--;
            if (posMap.find(end_key) == posMap.end())
            {
                VERBOSE(VB_IMPORTANT, "Couldn't find keyframe to end on");
                continue;
            }

            VERBOSE(VB_FILE, QString("Saving from frame #%1 to end of GOP"
                                     " starting at frame #%2")
                                     .arg(start_key).arg(end_key));

            /* seek to end gop */
            av_read_frame_flush(inputFC);
            url_fseek(&inputFC->pb, posMap[end_key], SEEK_SET);

            /* read in last gop of the save section */
            end_pts = AV_NOPTS_VALUE;
            pkts = ReadGop(video_index, NULL, NULL, &end_pts);
            FreeGop(pkts);

            /* seek to start gop */
            av_read_frame_flush(inputFC);
            url_fseek(&inputFC->pb, posMap[start_key], SEEK_SET);

            /* read in the first gop of the save section */
            delta_pts = AV_NOPTS_VALUE;
            start_pts = AV_NOPTS_VALUE;
            pkts = ReadGop(video_index, &min_dts, &start_pts, NULL);
            FreeGop(pkts);

            /* calcuate actual delta used in pts/dts fixup */
            if (next_start_pts == (int64_t)AV_NOPTS_VALUE)
            {
                /* first section */
                /* minimum dts in first gop should translate to a dts of 0 */
                delta_pts = min_dts;
            }
            else
            {
                /* not first section */
                /* minimum pts of video_index should translate to next_start_pts */
                delta_pts = start_pts - next_start_pts;
            }
            next_start_pts = end_pts;
            

            VERBOSE(VB_FILE, QString("Minimum PTS == %1, Maximum PTS == %2,"
                                     " I/O Delta == %3")
                                     .arg(start_pts).arg(end_pts)
                                     .arg(delta_pts));

            /* seek to start gop */
            av_read_frame_flush(inputFC);
            url_fseek(&inputFC->pb, posMap[start_key], SEEK_SET);

            /* repeat until pts of all streams are past end_pts */
            bool done;
            bool *dones = new bool[inputFC->nb_streams];
            for (int i = 0; i < inputFC->nb_streams; i++) dones[i] = false;

            int percentage = -1;
            do
            {
                /* read packet */
                ret = av_read_frame(inputFC, &pkt);
                if (ret < 0) break;

                /* end pts is the pts of the last frame in the last gop + the duration of the frame */
                /* ie it is the pts of the first frame in the next gop after the cut point */
                /* if the current frame is to be decoded after end_pts we take this as an indication */
                /* that we are finished with this stream for the currect section */
                if (pkt.dts >= end_pts) dones[pkt.stream_index] = true;

                /* if the presentation time is between start_pts and end_pts save it in the output */
                if ((pkt.pts != (int64_t)AV_NOPTS_VALUE && pkt.pts >= start_pts && pkt.pts < end_pts) ||
                    (pkt.pts == (int64_t)AV_NOPTS_VALUE && pkt.dts >= min_dts && pkt.dts < end_pts))
                {
                    if (pkt.stream_index == video_index) completed_frames++;

                    if (pkt.pts != (int64_t)AV_NOPTS_VALUE) pkt.pts -= delta_pts;
                    if (pkt.dts != (int64_t)AV_NOPTS_VALUE) pkt.dts -= delta_pts;

                    VERBOSE(VB_FILE, QString("T: %1, %2").arg(pkt.pts).arg(pkt.dts));

                    av_interleaved_write_frame(outputFC, &pkt);
                }
                else if (pkt.destruct) pkt.destruct(&pkt);

                done = true;
                for (int i = 0; i < inputFC->nb_streams; i++) done &= dones[i];

                int new_percentage = 100 * completed_frames/total_frames;
                if (new_percentage != percentage)
                {
                    percentage = new_percentage;
                    if (chkTranscodeDB)
                    {
                        if (JobQueue::GetJobCmd(jobID) == JOB_STOP)
                        {
                            unlink(tmpfile.ascii());
                            /* should probably clean up here */
                            VERBOSE(VB_IMPORTANT, "Transcoding STOPped by JobQueue");
                            return TRANSCODE_EXIT_STOPPED;
                        }
                        JobQueue::ChangeJobComment(jobID, QString("%1% ").arg(percentage) +
                                                          QObject::tr("Completed"));
                    }
                    else
                        cerr << "Percent complete: " <<  percentage << "%\r" << flush;
                }

            } while (!done);
        }
        else    // delete frames
        {
            // skip frames
            dMapIter++;
            if (dMapIter == deleteMap.end()) continue;
        }
    }

    /*============== Close AV ==============*/
    // Close output file
    url_fclose(&outputFC->pb);
    for (int i = 0; i < outputFC->nb_streams; i++) av_free(outputFC->streams[i]);
    av_free(outputFC);
    outputFC = NULL;

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    return TRANSCODE_EXIT_OK;
}

int MPEG2trans::BuildKeyframeIndex(QString &file, PositionMap &posMap)
{
    int jobID = -1;
    if (chkTranscodeDB)
    {
        if (JobQueue::GetJobCmd(jobID) == JOB_STOP)
        {
            unlink(file.ascii());
            /* should probably clean up here */
            VERBOSE(VB_IMPORTANT, "Transcoding STOPped by JobQueue");
            return TRANSCODE_EXIT_STOPPED;
        }
        JobQueue::ChangeJobComment(jobID, QString(QObject::tr("Generating Keyframe Index")));
    }
    else
        VERBOSE(VB_IMPORTANT, "Generating Keyframe Index");

    AVPacket pkt;
    int video_index = -1;
    int count = 0;

    /*============ initialise AV ===============*/
    if (!InitAV(file))
        return TRANSCODE_EXIT_UNKNOWN_ERROR;

    /* find video stream index */
    for (int i = 0; i < inputFC->nb_streams; i++)
    {
        if (inputFC->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    /* no video stream */
    if (video_index == -1)
        return TRANSCODE_BUGGY_EXIT_INVALID_VIDEO;

    av_init_packet(&pkt);

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == video_index)
        {
            if (pkt.flags & PKT_FLAG_KEY)
                posMap[count] = pkt.pos;
            count++;
        }
    }

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    if (chkTranscodeDB)
        JobQueue::ChangeJobComment(jobID, QString(QObject::tr("Transcode Completed")));
    else
        VERBOSE(VB_IMPORTANT, "Transcode Completed");

    return TRANSCODE_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
