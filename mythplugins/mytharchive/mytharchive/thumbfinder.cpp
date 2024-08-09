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

// c++
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>

// qt
#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QPainter>

// myth
#include <mythconfig.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythmiscutil.h> // for MythFile::copy
#include <libmythbase/programinfo.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythimage.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

extern "C" {
    #include <libavutil/imgutils.h>
}

#ifndef INT64_C    // Used in FFmpeg headers to define some constants
#define INT64_C(v)   (v ## LL)
#endif

// mytharchive
#include "thumbfinder.h"

// the amount to seek before the required frame
static constexpr int8_t PRE_SEEK_AMOUNT { 50 };

static const std::array<const SeekAmount,9> kSeekAmounts
{{
    {"frame",       -1},
    {"1 second",     1},
    {"5 seconds",    5},
    {"10 seconds",  10},
    {"30 seconds",  30},
    {"1 minute",    60},
    {"5 minutes",  300},
    {"10 minutes", 600},
    {"Cut Point",   -2},
}};

ThumbFinder::ThumbFinder(MythScreenStack *parent, ArchiveItem *archiveItem,
                         const QString &menuTheme)
            :MythScreenType(parent, "ThumbFinder"),
    m_archiveItem(archiveItem),
    m_thumbCount(getChapterCount(menuTheme)),
    m_thumbDir(createThumbDir())
{
    // copy thumbList so we can abandon changes if required
    m_thumbList.clear();
    for (const auto *item : std::as_const(m_archiveItem->thumbList))
    {
        auto *thumb = new ThumbImage;
        *thumb = *item;
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
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mythburn-ui.xml", "thumbfinder", this);
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

    connect(m_imageGrid, &MythUIButtonList::itemSelected,
            this, &ThumbFinder::gridItemChanged);

    connect(m_saveButton, &MythUIButton::Clicked, this, &ThumbFinder::savePressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &ThumbFinder::cancelPressed);

    connect(m_frameButton, &MythUIButton::Clicked, this, &ThumbFinder::updateThumb);

    m_seekAmountText->SetText(kSeekAmounts[m_currentSeek].name);

    BuildFocusList();

    SetFocusWidget(m_imageGrid);

    return true;
}

bool ThumbFinder::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            NextPrevWidgetFocus(true);
            return true;
        }

        if (action == "ESCAPE")
        {
            ShowMenu();
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
            {
                handled = false;
            }
        }
        else
        {
            handled = false;
        }
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
        progInfo->QueryCutList(m_deleteMap);
    delete progInfo;

    if (m_deleteMap.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "ThumbFinder::loadCutList: Got an empty delete map");
        return;
    }

    // if the first mark is a end mark then add the start mark at the beginning
    frm_dir_map_t::const_iterator it = m_deleteMap.constBegin();
    if (it.value() == MARK_CUT_END)
        m_deleteMap.insert(0, MARK_CUT_START);


    // if the last mark is a start mark then add the end mark at the end
    it = m_deleteMap.constEnd();
    --it;
    if (it != m_deleteMap.constEnd())
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

    for (const auto *item : std::as_const(m_thumbList))
    {
        auto *thumb = new ThumbImage;
        *thumb = *item;
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
        if (m_currentSeek >= kSeekAmounts.size())
            m_currentSeek = 0;
    }
    else
    {
        if (m_currentSeek == 0)
            m_currentSeek = kSeekAmounts.size();
        m_currentSeek--;
    }

    m_seekAmountText->SetText(kSeekAmounts[m_currentSeek].name);
}

void ThumbFinder::gridItemChanged([[maybe_unused]] MythUIButtonListItem *item)
{
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
        if( chmod(qPrintable(thumbDir), 0777) != 0 )
            LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: Failed to change permissions"
                                     " on thumb directory: " + ENO);
    }

    QString path;
    for (int x = 1; dir.exists(); x++)
    {
        path = thumbDir + QString("/%1").arg(x);
        dir.setPath(path);
    }

    dir.mkdir(path);
    if( chmod(qPrintable(path), 0777) != 0 )
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
    MythFile::copy(dst, src);

    item->SetImage(imageFile, "", true);

    // update the image grid item
    int64_t pos = (m_currentPTS - m_startPTS) / m_frameTime;
    thumb->frame = pos - m_offset;
    if (itemNo != 0)
    {
        thumb->caption = frameToTime(thumb->frame);
        item->SetText(thumb->caption);
    }

    m_imageGrid->SetRedraw();
}

QString ThumbFinder::frameToTime(int64_t frame, bool addFrame) const
{
    int sec = (int) (frame / m_fps);
    frame = frame - (int) (sec * m_fps);

    QString str = MythDate::formatTime(std::chrono::seconds(sec), "HH:mm:ss");
    if (addFrame)
        str += QString(".%1").arg(frame,10,2,QChar('0'));
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

    int chapterLen = 0;
    if (m_thumbCount)
        chapterLen = m_finalDuration / m_thumbCount;
    else
        chapterLen = m_finalDuration;

    m_updateFrame = false;

    // add title thumb
    m_frameFile = m_thumbDir + "/title.jpg";
    ThumbImage *thumb = nullptr;

    if (!m_thumbList.empty())
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
    {
        m_frameFile = thumb->filename;
    }

    seekToFrame(thumb->frame);
    getFrameImage();

    new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);

    QCoreApplication::processEvents();

    for (int x = 1; x <= m_thumbCount; x++)
    {
        m_frameFile = m_thumbDir + QString("/chapter-%1.jpg").arg(x);

        thumb = nullptr;

        if (m_archiveItem->thumbList.size() > x)
        {
            // use the thumb details in the archiveItem if already available
            thumb = m_archiveItem->thumbList.at(x);
        }

        if (!thumb)
        {
            int chapter = chapterLen * (x - 1);
            int hour = chapter / 3600;
            int min = (chapter % 3600) / 60;
            int sec = chapter % 60;
            QString time = QString::asprintf("%02d:%02d:%02d", hour, min, sec);

            auto frame = (int64_t) (chapter * ceil(m_fps));

            // no thumb available create a new one
            thumb = new ThumbImage;
            thumb->filename = m_frameFile;
            thumb->frame = frame;
            thumb->caption = time;
            m_thumbList.append(thumb);
        }
        else
        {
            m_frameFile = thumb->filename;
        }

        seekToFrame(thumb->frame);
        QCoreApplication::processEvents();
        getFrameImage();
        QCoreApplication::processEvents();
        new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);
        QCoreApplication::processEvents();
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
    // Open recording
    LOG(VB_JOBQUEUE, LOG_INFO, QString("ThumbFinder: Opening '%1'")
            .arg(inFile));

    if (!m_inputFC.Open(inFile))
    {
        LOG(VB_GENERAL, LOG_ERR, "ThumbFinder, Couldn't open input file" + ENO);
        return false;
    }

    // Getting stream information
    int ret = avformat_find_stream_info(m_inputFC, nullptr);
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
        if (m_inputFC->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
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
            m_frameWidth = st->codecpar->width;
            m_frameHeight = st->codecpar->height;
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
    m_codecCtx = m_codecMap.GetCodecContext(m_inputFC->streams[m_videostream]);
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

    if (m_codec == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ThumbFinder: Couldn't find codec for video stream");
        return false;
    }

    // open codec
    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "ThumbFinder: Couldn't open codec for video stream");
        return false;
    }

    // allocate temp buffer
    int bufflen = m_frameWidth * m_frameHeight * 4;
    m_outputbuf = new unsigned char[bufflen];
    m_frameFile = getTempDirectory() + "work/frame.jpg";
    return true;
}

int ThumbFinder::checkFramePosition(int frameNumber)
{
    if (m_deleteMap.isEmpty() || !m_archiveItem->useCutlist)
        return frameNumber;

    int diff = 0;
    frm_dir_map_t::const_iterator it = m_deleteMap.constFind(frameNumber);

    for (it = m_deleteMap.constBegin(); it != m_deleteMap.constEnd(); ++it)
    {
        int start = it.key();

        ++it;
        if (it == m_deleteMap.constEnd())
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

    timestamp = std::max(timestamp, m_startTime);

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
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;

    int inc = kSeekAmounts[m_currentSeek].amount;

    if (inc == -1)
        inc = 1;
    else if (inc == -2)
    {
        int pos = 0;
        frm_dir_map_t::const_iterator it;
        for (it = m_deleteMap.constBegin(); it != m_deleteMap.constEnd(); ++it)
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
    {
        inc = (int) (inc * ceil(m_fps));
    }

    int64_t newFrame = currentFrame + inc - m_offset;
    if (newFrame == currentFrame + 1)
        getFrameImage(false);
    else
        seekToFrame(newFrame);

    return true;
}

bool ThumbFinder::seekBackward()
{
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;

    int inc = kSeekAmounts[m_currentSeek].amount;
    if (inc == -1)
        inc = -1;
    else if (inc == -2)
    {
        // seek to previous cut point
        frm_dir_map_t::const_iterator it;
        int pos = 0;
        for (it = m_deleteMap.constBegin(); it != m_deleteMap.constEnd(); ++it)
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
    {
        inc = (int) (-inc * ceil(m_fps));
    }

    int64_t newFrame = currentFrame + inc - m_offset;
    seekToFrame(newFrame);

    return true;
}

bool ThumbFinder::getFrameImage(bool needKeyFrame, int64_t requiredPTS)
{
    AVFrame orig;
    AVFrame retbuf;
    memset(&orig, 0, sizeof(AVFrame));
    memset(&retbuf, 0, sizeof(AVFrame));

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return false;
    }

    bool frameFinished = false;
    bool gotKeyFrame = false;

    while (av_read_frame(m_inputFC, pkt) >= 0 && !frameFinished)
    {
        if (pkt->stream_index == m_videostream)
        {
            int keyFrame = pkt->flags & AV_PKT_FLAG_KEY;

            if (m_startPTS == -1 && pkt->dts != AV_NOPTS_VALUE)
            {
                m_startPTS = pkt->dts;
                m_frameTime = pkt->duration;
            }

            if (keyFrame)
                gotKeyFrame = true;

            if (!gotKeyFrame && needKeyFrame)
            {
                av_packet_unref(pkt);
                continue;
            }

            if (m_firstIFramePTS == -1)
                m_firstIFramePTS = pkt->dts;

            av_frame_unref(m_frame);
            frameFinished = false;
            int ret = avcodec_receive_frame(m_codecCtx, m_frame);
            if (ret == 0)
                frameFinished = true;
            if (ret == 0 || ret == AVERROR(EAGAIN))
                avcodec_send_packet(m_codecCtx, pkt);
            if (requiredPTS != -1 && pkt->dts != AV_NOPTS_VALUE && pkt->dts < requiredPTS)
                frameFinished = false;

            m_currentPTS = pkt->dts;
        }

        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    if (frameFinished)
    {
        av_image_fill_arrays(retbuf.data, retbuf.linesize, m_outputbuf,
            AV_PIX_FMT_RGB32, m_frameWidth, m_frameHeight, IMAGE_ALIGN);
        AVFrame *tmp = m_frame;
        MythAVUtil::DeinterlaceAVFrame(m_frame);
        m_copy.Copy(&retbuf, AV_PIX_FMT_RGB32, tmp, m_codecCtx->pix_fmt,
                    m_frameWidth, m_frameHeight);

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
                GetMythMainWindow()->GetPainter()->GetFormatImage();
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
    delete[] m_outputbuf;

    // close the codec
    if (m_inputFC.isOpen() && m_inputFC->streams)
        m_codecMap.FreeCodecContext(m_inputFC->streams[m_videostream]);

    // close the video file
    m_inputFC.Close();
}

void ThumbFinder::ShowMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Exit, Save Thumbnails"), &ThumbFinder::savePressed);
    menuPopup->AddButton(tr("Exit, Don't Save Thumbnails"), &ThumbFinder::cancelPressed);
}

void ThumbFinder::updatePositionBar(int64_t frame)
{
    if (!m_positionImage)
        return;

    QSize size = m_positionImage->GetArea().size();
    auto *pixmap = new QPixmap(size.width(), size.height());

    QPainter p(pixmap);
    QBrush brush(Qt::green);

    p.setBrush(brush);
    p.setPen(Qt::NoPen);
    p.fillRect(0, 0, size.width(), size.height(), brush);

    frm_dir_map_t::const_iterator it;

    brush.setColor(Qt::red);

    for (it = m_deleteMap.constBegin(); it != m_deleteMap.constEnd(); ++it)
    {
        double startdelta = size.width();
        if (it.key() != 0)
            startdelta = (m_archiveItem->duration * m_fps) / it.key();

        ++it;
        if (it == m_deleteMap.constEnd())
        {
            LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: found a start cut but no cut end");
            break;
        }

        double enddelta = size.width();
        if (it.key() != 0)
            enddelta = (m_archiveItem->duration * m_fps) / it.key();

        int start = (int) (size.width() / startdelta);
        int end = (int) (size.width() / enddelta);
        p.fillRect(start - 1, 0, end - start, size.height(), brush);
    }

    if (frame == 0)
        frame = 1;
    brush.setColor(Qt::yellow);
    int pos = (int) (size.width() / ((m_archiveItem->duration * m_fps) / frame));
    p.fillRect(pos, 0, 3, size.height(), brush);

    MythImage *image = GetMythMainWindow()->GetPainter()->GetFormatImage();
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

            int cutLen = 0;

            for (it = m_deleteMap.constBegin(); it != m_deleteMap.constEnd(); ++it)
            {
                int start = it.key();

                ++it;
                if (it == m_deleteMap.constEnd())
                {
                    LOG(VB_GENERAL, LOG_ERR, "ThumbFinder: found a start cut but no cut end");
                    break;
                }

                int end = it.key();
                cutLen += end - start;
            }
            return m_archiveItem->duration - (int) (cutLen / m_fps);
        }
    }

    return m_archiveItem->duration;
}
