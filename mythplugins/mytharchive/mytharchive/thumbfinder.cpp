/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2009, Janne Grunau <janne-mythtv@grunau.be>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// c
#include <sys/stat.h>
#include <math.h>
#include <errno.h>

// c++
#include <cstdlib>
#include <iostream>

// qt
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QDir>
#include <QPainter>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <programinfo.h>
#include <mythuihelper.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythdirs.h>
#include <mythmiscutil.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuiimage.h>
#include <mythuibuttonlist.h>
#include <mythimage.h>
#include <mythconfig.h>
extern "C" {
#include <swscale.h>
}

#ifndef INT64_C    // Used in FFmpeg headers to define some constants
#define INT64_C(v)   (v ## LL)
#endif

// mytharchive
#include "thumbfinder.h"

// the amount to seek before the required frame
#define PRE_SEEK_AMOUNT 50

struct SeekAmount SeekAmounts[] =
{
    {"frame",       -1},
    {"1 second",     1},
    {"5 seconds",    5},
    {"10 seconds",  10},
    {"30 seconds",  30},
    {"1 minute",    60},
    {"5 minutes",  300},
    {"10 minutes", 600},
    {"Cut Point",   -2},
};

int SeekAmountsCount = sizeof(SeekAmounts) / sizeof(SeekAmounts[0]);

ThumbFinder::ThumbFinder(MythScreenStack *parent, ArchiveItem *archiveItem,
                         const QString &menuTheme)
            :MythScreenType(parent, "ThumbFinder"),
    m_inputFC(NULL),        m_codecCtx(NULL),
    m_codec(NULL),          m_frame(NULL),
    m_fps(0.0),             m_outputbuf(NULL),
    m_frameWidth(0),        m_frameHeight(0),
    m_videostream(0),       m_currentSeek(0),
    m_startTime(-1),        m_startPTS(-1),
    m_currentPTS(-1),       m_firstIFramePTS(-1),
    m_frameTime(0),         m_updateFrame(false),
    m_finalDuration(0),     m_offset(0),
    m_archiveItem(archiveItem),
    m_thumbCount(getChapterCount(menuTheme)),
    m_thumbDir(createThumbDir()),
    m_frameButton(NULL),    m_saveButton(NULL),
    m_cancelButton(NULL),   m_frameImage(NULL),
    m_positionImage(NULL),  m_imageGrid(NULL),
    m_seekAmountText(NULL), m_currentPosText(NULL)
{
    // copy thumbList so we can abandon changes if required
    m_thumbList.clear();
    for (int x = 0; x < m_archiveItem->thumbList.size(); x++)
    {
        ThumbImage *thumb = new ThumbImage;
        *thumb = *m_archiveItem->thumbList.at(x);
        m_thumbList.append(thumb);
    }
}

void ThumbFinder::Init(void)
{
    getThumbImages();
}

ThumbFinder::~ThumbFinder()
{
    while (!m_thumbList.isEmpty())
         delete m_thumbList.takeFirst();
    m_thumbList.clear();

    closeAVCodec();
}

bool ThumbFinder::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "thumbfinder", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_frameImage, "frameimage", &err);
    UIUtilE::Assign(this, m_positionImage, "positionimage", &err);
    UIUtilE::Assign(this, m_imageGrid, "thumblist", &err);
    UIUtilE::Assign(this, m_saveButton, "save_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_frameButton, "frame_button", &err);
    UIUtilE::Assign(this, m_seekAmountText, "seekamount", &err);
    UIUtilE::Assign(this, m_currentPosText, "currentpos", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'mythburn'");
        return false;
    }

    connect(m_imageGrid, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(gridItemChanged(MythUIButtonListItem *)));

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(savePressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    connect(m_frameButton, SIGNAL(Clicked()), this, SLOT(updateThumb()));

    BuildFocusList();

    SetFocusWidget(m_imageGrid);

    return true;
}

bool ThumbFinder::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            NextPrevWidgetFocus(true);
            return true;
        }

        if (action == "ESCAPE")
        {
            showMenu();
            return true;
        }

        if (action == "0" || action == "1" || action == "2" || action == "3" ||
            action == "4" || action == "5" || action == "6" || action == "7" ||
            action == "8" || action == "9")
        {
            m_imageGrid->SetItemCurrent(action.toInt());
            int itemNo = m_imageGrid->GetCurrentPos();
            ThumbImage *thumb = m_thumbList.at(itemNo);
            if (thumb)
                seekToFrame(thumb->frame);
            return true;
        }

        if (GetFocusWidget() == m_frameButton)
        {
            if (action == "UP")
            {
                changeSeekAmount(true);
            }
            else if (action == "DOWN")
            {
                changeSeekAmount(false);
            }
            else if (action == "LEFT")
            {
                seekBackward();
            }
            else if (action == "RIGHT")
            {
                seekForward();
            }
            else if (action == "SELECT")
            {
                updateThumb();
            }
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

int  ThumbFinder::getChapterCount(const QString &menuTheme)
{
    QString filename = GetShareDir() + "mytharchive/themes/" +
            menuTheme + "/theme.xml";
    QDomDocument doc("mydocument");
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open theme file: " + filename);
        return 0; //??
    }
    if (!doc.setContent(&file))
    {
        file.close();
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse theme file: " + filename);
        return 0;
    }
    file.close();

    QDomNodeList chapterNodeList = doc.elementsByTagName("chapter");

    return chapterNodeList.count();
}

void ThumbFinder::loadCutList()
{
    ProgramInfo *progInfo = getProgramInfoForFile(m_archiveItem->filename);

    if (progInfo && m_archiveItem->hasCutlist)
    {
        progInfo->QueryCutList(m_deleteMap);
        delete progInfo;
    }

    // if the first mark is a end mark then add the start mark at the beginning
    frm_dir_map_t::const_iterator it = m_deleteMap.begin();
    if (it.value() == MARK_CUT_END)
        m_deleteMap.insert(0, MARK_CUT_START);


    // if the last mark is a start mark then add the end mark at the end
    it = m_deleteMap.end();
    --it;
    if (it != m_deleteMap.end())
    {
        if (it.value() == MARK_CUT_START)
            m_deleteMap.insert(m_archiveItem->duration * m_fps, MARK_CUT_END);
    }
}

void ThumbFinder::savePressed()
{
    // copy the thumb details to the archiveItem
    while (!m_archiveItem->thumbList.isEmpty())
         delete m_archiveItem->thumbList.takeFirst();
    m_archiveItem->thumbList.clear();

    for (int x = 0; x < m_thumbList.size(); x++)
    {
        ThumbImage *thumb = new ThumbImage;
        *thumb = *m_thumbList.at(x);
        m_archiveItem->thumbList.append(thumb);
    }

    Close();
}

void ThumbFinder::cancelPressed()
{
    Close();
}

void ThumbFinder::updateCurrentPos()
{
    int64_t pos = m_currentPTS - m_firstIFramePTS;
    int64_t frame = pos / m_frameTime;

    if (m_currentPosText)
        m_currentPosText->SetText(frameToTime(frame, true));

    updatePositionBar(frame);
}

void ThumbFinder::changeSeekAmount(bool up)
{
    if (up)
    {
        m_currentSeek++;
        if (m_currentSeek >= SeekAmountsCount)
            m_currentSeek = 0;
    }
    else
    {
        m_currentSeek--;
        if (m_currentSeek < 0)
            m_currentSeek = SeekAmountsCount - 1;
    }

    m_seekAmountText->SetText(SeekAmounts[m_currentSeek].name);
}

void ThumbFinder::gridItemChanged(MythUIButtonListItem *item)
{
    (void) item;

    int itemNo = m_imageGrid->GetCurrentPos();
    ThumbImage *thumb = m_thumbList.at(itemNo);
    if (thumb)
          seekToFrame(thumb->frame);
}

QString ThumbFinder::createThumbDir(void)
{
    QString thumbDir = getTempDirectory() + "config/thumbs";

    // make sure the thumb directory exists
    QDir dir(thumbDir);
    if (!dir.exists())
    {
        dir.mkdir(thumbDir);
        if( chmod(qPrintable(thumbDir), 0777) )
            LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: Failed to change permissions"
                                     " on thumb directory: " + ENO);
    }

    QString path;
    for (int x = 1; dir.exists(); x++)
    {
        path = QString(thumbDir + "/%1").arg(x);
        dir.setPath(path);
    }

    dir.mkdir(path);
    if( chmod(qPrintable(path), 0777) )
        LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: Failed to change permissions on "
                                 "thumb directory: %1" + ENO);

    return path;
}

void ThumbFinder::updateThumb(void)
{
    int itemNo = m_imageGrid->GetCurrentPos();
    MythUIButtonListItem *item = m_imageGrid->GetItemCurrent();

    ThumbImage *thumb = m_thumbList.at(itemNo);
    if (!thumb)
        return;

    // copy current frame image to the selected thumb image
    QString imageFile = thumb->filename;
    QFile dst(imageFile);
    QFile src(m_frameFile);
    copy(dst, src);

    item->SetImage(imageFile, "", true);

    // update the image grid item
    int64_t pos = (int) ((m_currentPTS - m_startPTS) / m_frameTime);
    thumb->frame = pos - m_offset;
    if (itemNo != 0)
    {
        thumb->caption = frameToTime(thumb->frame);
        item->SetText(thumb->caption);
    }

    m_imageGrid->SetRedraw();
}

QString ThumbFinder::frameToTime(int64_t frame, bool addFrame)
{
    int hour, min, sec;
    QString str;

    sec = (int) (frame / m_fps);
    frame = frame - (int) (sec * m_fps);
    min = sec / 60;
    sec %= 60;
    hour = min / 60;
    min %= 60;

    if (addFrame)
        str = str.sprintf("%01d:%02d:%02d.%02d", hour, min, sec, (int) frame);
    else
        str = str.sprintf("%02d:%02d:%02d", hour, min, sec);
    return str;
}

bool ThumbFinder::getThumbImages()
{
    if (!getFileDetails(m_archiveItem))
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("ThumbFinder:: Failed to get file details for %1")
                .arg(m_archiveItem->filename));
        return false;
    }

    if (!initAVCodec(m_archiveItem->filename))
        return false;

    if (m_archiveItem->type == "Recording")
        loadCutList();

    // calculate the file duration taking the cut list into account
    m_finalDuration = calcFinalDuration();

    QString origFrameFile = m_frameFile;

    m_updateFrame = true;
    getFrameImage();

    int chapterLen;
    if (m_thumbCount)
        chapterLen = m_finalDuration / m_thumbCount;
    else
        chapterLen = m_finalDuration;

    QString thumbList = "";
    m_updateFrame = false;

    // add title thumb
    m_frameFile = m_thumbDir + "/title.jpg";
    ThumbImage *thumb = NULL;

    if (m_thumbList.size() > 0)
    {
        // use the thumb details in the thumbList if already available
        thumb = m_thumbList.at(0);
    }

    if (!thumb)
    {
        // no thumb available create a new one
        thumb = new ThumbImage;
        thumb->filename = m_frameFile;
        thumb->frame = (int64_t) 0;
        thumb->caption = "Title";
        m_thumbList.append(thumb);
    }
    else
        m_frameFile = thumb->filename;

    seekToFrame(thumb->frame);
    getFrameImage();

    new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);

    qApp->processEvents();

    for (int x = 1; x <= m_thumbCount; x++)
    {
        m_frameFile = QString(m_thumbDir + "/chapter-%1.jpg").arg(x);

        thumb = NULL;

        if (m_archiveItem->thumbList.size() > x)
        {
            // use the thumb details in the archiveItem if already available
            thumb = m_archiveItem->thumbList.at(x);
        }

        if (!thumb)
        {
            QString time;
            int chapter, hour, min, sec;

            chapter = chapterLen * (x - 1);
            hour = chapter / 3600;
            min = (chapter % 3600) / 60;
            sec = chapter % 60;
            time = time.sprintf("%02d:%02d:%02d", hour, min, sec);

            int64_t frame = (int64_t) (chapter * ceil(m_fps));

            // no thumb available create a new one
            thumb = new ThumbImage;
            thumb->filename = m_frameFile;
            thumb->frame = frame;
            thumb->caption = time;
            m_thumbList.append(thumb);
        }
        else
            m_frameFile = thumb->filename;

        seekToFrame(thumb->frame);
        qApp->processEvents();
        getFrameImage();
        qApp->processEvents();
        new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);
        qApp->processEvents();
    }

    m_frameFile = origFrameFile;
    seekToFrame(0);

    m_updateFrame = true;

    m_imageGrid->SetRedraw();

    SetFocusWidget(m_imageGrid);

    return true;
}

bool ThumbFinder::initAVCodec(const QString &inFile)
{
    av_register_all();

    // Open recording
    LOG(VB_JOBQUEUE, LOG_INFO, QString("ThumbFinder: Opening '%1'")
            .arg(inFile));

    if (!m_inputFC.Open(inFile))
    {
        LOG(VB_GENERAL, LOG_ERR, "ThumbFinder, Couldn't open input file" + ENO);
        return false;
    }

    // Getting stream information
    int ret = avformat_find_stream_info(m_inputFC, NULL);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't get stream info, error #%1").arg(ret));
        return false;
    }

    av_dump_format(m_inputFC, 0, qPrintable(inFile), 0);

    // find the first video stream
    m_videostream = -1;

    for (uint i = 0; i < m_inputFC->nb_streams; i++)
    {
        AVStream *st = m_inputFC->streams[i];
        if (m_inputFC->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_startTime = -1;
            if (m_inputFC->streams[i]->start_time != (int) AV_NOPTS_VALUE)
                m_startTime = m_inputFC->streams[i]->start_time;
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "ThumbFinder: Failed to get start time");
                return false;
            }

            m_videostream = i;
            m_frameWidth = st->codec->width;
            m_frameHeight = st->codec->height;
            if (st->r_frame_rate.den && st->r_frame_rate.num)
                m_fps = av_q2d(st->r_frame_rate);
            else
                m_fps = 1/av_q2d(st->time_base);
            break;
        }
    }

    if (m_videostream == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't find a video stream");
        return false;
    }

    // get the codec context for the video stream
    m_codecCtx = m_inputFC->streams[m_videostream]->codec;
    m_codecCtx->debug_mv = 0;
    m_codecCtx->debug = 0;
    m_codecCtx->workaround_bugs = 1;
    m_codecCtx->lowres = 0;
    m_codecCtx->idct_algo = FF_IDCT_AUTO;
    m_codecCtx->skip_frame = AVDISCARD_DEFAULT;
    m_codecCtx->skip_idct = AVDISCARD_DEFAULT;
    m_codecCtx->skip_loop_filter = AVDISCARD_DEFAULT;
    m_codecCtx->err_recognition = AV_EF_CAREFUL;
    m_codecCtx->error_concealment = 3;

    // get decoder for video stream
    m_codec = avcodec_find_decoder(m_codecCtx->codec_id);

    if (m_codec == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ThumbFinder: Couldn't find codec for video stream");
        return false;
    }

    // open codec
    if (avcodec_open2(m_codecCtx, m_codec, NULL) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ThumbFinder: Couldn't open codec for video stream");
        return false;
    }

    // allocate temp buffer
    int bufflen = m_frameWidth * m_frameHeight * 4;
    m_outputbuf = new unsigned char[bufflen];

    m_frame = avcodec_alloc_frame();

    m_frameFile = getTempDirectory() + "work/frame.jpg";

    return true;
}

int ThumbFinder::checkFramePosition(int frameNumber)
{
    if (m_deleteMap.isEmpty() || !m_archiveItem->useCutlist)
        return frameNumber;

    int diff = 0;
    frm_dir_map_t::const_iterator it = m_deleteMap.find(frameNumber);

    for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
    {
        int start = it.key();

        ++it;
        if (it == m_deleteMap.end())
        {
            LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: found a start cut but no cut end");
            break;
        }

        int end = it.key();

        if (start <= frameNumber + diff)
            diff += end - start;
    }

    m_offset = diff;
    return frameNumber + diff;
}

bool ThumbFinder::seekToFrame(int frame, bool checkPos)
{
    // make sure the frame is not in a cut point
    if (checkPos)
        frame = checkFramePosition(frame);

    // seek to a position PRE_SEEK_AMOUNT frames before the required frame
    int64_t timestamp = m_startTime + (frame * m_frameTime) -
                        (PRE_SEEK_AMOUNT * m_frameTime);
    int64_t requiredPTS = m_startPTS + (frame * m_frameTime);

    if (timestamp < m_startTime)
        timestamp = m_startTime;

    if (av_seek_frame(m_inputFC, m_videostream, timestamp, AVSEEK_FLAG_ANY) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "ThumbFinder::SeekToFrame: seek failed") ;
        return false;
    }

    avcodec_flush_buffers(m_codecCtx);
    getFrameImage(true, requiredPTS);

    return true;
}

bool ThumbFinder::seekForward()
{
    int inc;
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;
    int64_t newFrame;

    inc = SeekAmounts[m_currentSeek].amount;

    if (inc == -1)
        inc = 1;
    else if (inc == -2)
    {
        int pos = 0;
        frm_dir_map_t::const_iterator it;
        for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
        {
            if (it.key() > (uint64_t)currentFrame)
            {
                pos = it.key();
                break;
            }
        }
        // seek to next cutpoint
        m_offset = 0;
        seekToFrame(pos, false);
        return true;
    }
    else
        inc = (int) (inc * ceil(m_fps));

    newFrame = currentFrame + inc - m_offset;
    if (newFrame == currentFrame + 1)
        getFrameImage(false);
    else
        seekToFrame(newFrame);

    return true;
}

bool ThumbFinder::seekBackward()
{
    int inc;
    int64_t newFrame;
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;

    inc = SeekAmounts[m_currentSeek].amount;
    if (inc == -1)
        inc = -1;
    else if (inc == -2)
    {
        // seek to previous cut point
        frm_dir_map_t::const_iterator it;
        int pos = 0;
        for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
        {
            if (it.key() >= (uint64_t)currentFrame)
                break;

            pos = it.key();
        }

        // seek to next cutpoint
        m_offset = 0;
        seekToFrame(pos, false);
        return true;
    }
    else
        inc = (int) (-inc * ceil(m_fps));

    newFrame = currentFrame + inc - m_offset;
    seekToFrame(newFrame);

    return true;
}

// Note: copied this function from myth_imgconvert.cpp -- dtk 2009-08-17
static int myth_sws_img_convert(
    AVPicture *dst, PixelFormat dst_pix_fmt, AVPicture *src,
    PixelFormat pix_fmt, int width, int height)
{
    static QMutex lock;
    QMutexLocker locker(&lock);

    static struct SwsContext *convert_ctx;

    convert_ctx = sws_getCachedContext(convert_ctx, width, height, pix_fmt,
                                       width, height, dst_pix_fmt,
                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!convert_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, "myth_sws_img_convert: Cannot initialize "
                                 "the image conversion context");
        return -1;
    }

    sws_scale(convert_ctx, src->data, src->linesize,
              0, height, dst->data, dst->linesize);

    return 0;
}

bool ThumbFinder::getFrameImage(bool needKeyFrame, int64_t requiredPTS)
{
    AVPacket pkt;
    AVPicture orig;
    AVPicture retbuf;
    memset(&orig, 0, sizeof(AVPicture));
    memset(&retbuf, 0, sizeof(AVPicture));

    av_init_packet(&pkt);

    int frameFinished = 0;
    int keyFrame;
    int frameCount = 0;
    bool gotKeyFrame = false;

    while (av_read_frame(m_inputFC, &pkt) >= 0 && !frameFinished)
    {
        if (pkt.stream_index == m_videostream)
        {
            frameCount++;

            keyFrame = pkt.flags & AV_PKT_FLAG_KEY;

            if (m_startPTS == -1 && pkt.dts != (int64_t)AV_NOPTS_VALUE)
            {
                m_startPTS = pkt.dts;
                m_frameTime = pkt.duration;
            }

            if (keyFrame)
                gotKeyFrame = true;

            if (!gotKeyFrame && needKeyFrame)
            {
                av_free_packet(&pkt);
                continue;
            }

            if (m_firstIFramePTS == -1)
                m_firstIFramePTS = pkt.dts;

            avcodec_decode_video2(m_codecCtx, m_frame, &frameFinished, &pkt);

            if (requiredPTS != -1 && pkt.dts != (int64_t)AV_NOPTS_VALUE && pkt.dts < requiredPTS)
                frameFinished = false;

            m_currentPTS = pkt.dts;
        }

        av_free_packet(&pkt);
    }

    if (frameFinished)
    {
        avpicture_fill(&retbuf, m_outputbuf, PIX_FMT_RGB32, m_frameWidth, m_frameHeight);

        avpicture_deinterlace((AVPicture*)m_frame, (AVPicture*)m_frame,
                                m_codecCtx->pix_fmt, m_frameWidth, m_frameHeight);

        myth_sws_img_convert(
                    &retbuf, PIX_FMT_RGB32,
                        (AVPicture*) m_frame, m_codecCtx->pix_fmt, m_frameWidth, m_frameHeight);

        QImage img(m_outputbuf, m_frameWidth, m_frameHeight,
                   QImage::Format_RGB32);

        QByteArray ffile = m_frameFile.toLocal8Bit();
        if (!img.save(ffile.constData(), "JPEG"))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to save thumb: " + m_frameFile);
        }

        if (m_updateFrame)
        {
            MythImage *mimage =
                GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();
            mimage->Assign(img);
            m_frameImage->SetImage(mimage);
            mimage->DecrRef();
        }

        updateCurrentPos();
    }

    return true;
}

void ThumbFinder::closeAVCodec()
{
    if (m_outputbuf)
        delete[] m_outputbuf;

    // free the frame
    av_free(m_frame);

    // close the codec
    if (m_codecCtx)
        avcodec_close(m_codecCtx);

    // close the video file
    m_inputFC.Close();
}

void ThumbFinder::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Exit, Save Thumbnails"), SLOT(savePressed()));
    menuPopup->AddButton(tr("Exit, Don't Save Thumbnails"), SLOT(cancelPressed()));
}

void ThumbFinder::updatePositionBar(int64_t frame)
{
    if (!m_positionImage)
        return;

    QSize size = m_positionImage->GetArea().size();
    QPixmap *pixmap = new QPixmap(size.width(), size.height());

    QPainter p(pixmap);
    QBrush brush(Qt::green);

    p.setBrush(brush);
    p.setPen(Qt::NoPen);
    p.fillRect(0, 0, size.width(), size.height(), brush);

    frm_dir_map_t::const_iterator it;

    brush.setColor(Qt::red);
    double startdelta, enddelta;

    for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
    {
        if (it.key() != 0)
            startdelta = (m_archiveItem->duration * m_fps) / it.key();
        else
            startdelta = size.width();

        ++it;
        if (it == m_deleteMap.end())
        {
            LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: found a start cut but no cut end");
            break;
        }

        if (it.key() != 0)
            enddelta = (m_archiveItem->duration * m_fps) / it.key();
        else
            enddelta = size.width();
        int start = (int) (size.width() / startdelta);
        int end = (int) (size.width() / enddelta);
        p.fillRect(start - 1, 0, end - start, size.height(), brush);
    }

    if (frame == 0)
        frame = 1;
    brush.setColor(Qt::yellow);
    int pos = (int) (size.width() / ((m_archiveItem->duration * m_fps) / frame));
    p.fillRect(pos, 0, 3, size.height(), brush);

    MythImage *image = GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();
    image->Assign(*pixmap);
    m_positionImage->SetImage(image);

    p.end();
    delete pixmap;
}

int ThumbFinder::calcFinalDuration()
{
    if (m_archiveItem->type == "Recording")
    {
        if (m_archiveItem->useCutlist)
        {
            frm_dir_map_t::const_iterator it;

            int start, end, cutLen = 0;

            for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
            {
                start = it.key();

                ++it;
                if (it == m_deleteMap.end())
                {
                    LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: found a start cut but no cut end");
                    break;
                }

                end = it.key();
                cutLen += end - start;
            }
            return m_archiveItem->duration - (int) (cutLen / m_fps);
        }
    }

    return m_archiveItem->duration;
}

