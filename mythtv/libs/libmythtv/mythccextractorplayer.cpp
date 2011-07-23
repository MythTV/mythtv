// -*- Mode: c++ -*-

/*
 *  Class MythCCExtractorPlayer
 *
 *  Copyright (C) Digital Nirvana, Inc. 2010
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
using namespace std;

#include <QPainter>

#include "mythccextractorplayer.h"

MythCCExtractorPlayer::~MythCCExtractorPlayer()
{
    QHash<uint, CC708Reader*>::iterator it7 = cc708.begin();
    for (; it7 != cc708.end(); ++it7)
        delete *it7;
    cc708.clear();
    //
    QHash<uint, CC608Reader*>::iterator it6 = cc608.begin();
    for (; it6 != cc608.end(); ++it6)
        delete *it6;
    cc608.clear();
    //
    QHash<uint, SubtitleReader*>::iterator its = subReader.begin();
    for (; its != subReader.end(); ++its)
        delete *its;
    subReader.clear();
    //
    QHash<uint, TeletextExtractorReader *>::iterator itt = ttxReader.begin();
    for (; itt != ttxReader.end(); ++itt)
        delete *itt;
    ttxReader.clear();
}

/**
 * Call it when you got new video frame to process subtitles if any.
 */

void MythCCExtractorPlayer::OnGotNewFrame(void)
{
    m_myFramesPlayed = decoder->GetFramesRead();

    videoOutput->StartDisplayingFrame();
    VideoFrame *frame = videoOutput->GetLastShownFrame();
    videoOutput->DoneDisplayingFrame(frame);

    if (m_curTimeShift < 0)
        m_curTimeShift = frame->timecode;

    m_curTime = frame->timecode - m_curTimeShift;

    ProcessTTXSubtitles();
    Process608Subtitles();
    Process708Subtitles();
    ProcessDVBSubtitles();
}

/**
 * Call it when you finished reading of video to finish subtitles processing.
 */

void MythCCExtractorPlayer::OnDecodingFinished(void)
{
    FinishTTXSubtitlesProcess();
    Finish608SubtitlesProcess();
    Finish708SubtitlesProcess();
    FinishDVBSubtitlesProcess();
}

static QString progress_string(
    MythTimer &flagTime, uint64_t m_myFramesPlayed, uint64_t totalFrames)
{
    if (totalFrames == 0ULL)
    {
        return QString("%1 frames processed    \r")
            .arg(m_myFramesPlayed,7);
    }

    double elapsed = flagTime.elapsed() * 0.001;
    double flagFPS = (elapsed > 0.0) ? (m_myFramesPlayed / elapsed) : 0;

    double percentage = m_myFramesPlayed * 100.0 / totalFrames;
    percentage = (percentage > 100.0 && percentage < 101.0) ?
        100.0 : percentage;

    if (flagFPS < 10.0)
    {
        return QString("%1 fps %2%     \r")
            .arg(flagFPS,4,'f',1).arg(percentage,4,'f',1);
    }
    else
    {
        return QString("%1 fps %2%     \r")
            .arg(flagFPS,4,'f',0).arg(percentage,4,'f',1);
    }
}

bool MythCCExtractorPlayer::run(void)
{
    m_myFramesPlayed = 0;

    killdecoder = false;
    framesPlayed = 0;
    using_null_videoout = true;

    decoder->SetDecodeAllSubtitles(true);

    SetPlaying(true);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to initialize video");
        SetPlaying(false);
        return false;
    }

    ClearAfterSeek();

    MythTimer flagTime, ui_timer, inuse_timer, save_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();
    save_timer.start();

    m_curTime = 0;

    if (DecoderGetFrame(kDecodeVideo))
        OnGotNewFrame();

    if (m_showProgress)
        cout << "\r                                      \r" << flush;

    while (!GetEof())
    {
        if (inuse_timer.elapsed() > 2534)
        {
            inuse_timer.restart();
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (player_ctx->playingInfo)
                player_ctx->playingInfo->UpdateInUseMark();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (m_showProgress && (ui_timer.elapsed() > 98 * 4))
        {
            ui_timer.restart();
            QString str = progress_string(
                flagTime, m_myFramesPlayed, totalFrames);
            cout << qPrintable(str) << '\r' << flush;
        }

        if (DecoderGetFrame(kDecodeVideo))
        {
            OnGotNewFrame();
        }
    }

    if (m_showProgress)
    {
        if ((m_myFramesPlayed < totalFrames) &&
            ((m_myFramesPlayed + 30) > totalFrames))
        {
            m_myFramesPlayed = totalFrames;
        }
        QString str = progress_string(flagTime, m_myFramesPlayed, totalFrames);
        cout << qPrintable(str) << endl;
    }

    OnDecodingFinished();

    SetPlaying(false);
    killdecoder = true;

    return true;
}

/**
 * Processes a new subtitle.
 */

void MythCCExtractorPlayer::GotNew608Subtitle(uint streamId, int ccIdx,
                                              CC608Buffer *subtitles)
{
    QStringList content;
    for (int i = 0; i < (int) subtitles->buffers.size(); ++i)
    {
        content << subtitles->buffers[i]->text;
    }

    // Some workaround to eliminate 'HE' in English subtitles.
    if (content.size() > 0 && content.first() == "HE")
        content.removeFirst();

    QList<OneSubtitle> &list = cc608Subtitles[streamId][ccIdx];

    ProcessNewSubtitle(content, list);
}

/**
 * Processes new subtitles if any from all CC608 streams.
 */

void MythCCExtractorPlayer::Process608Subtitles(void)
{
    foreach (uint streamId, GetCC608StreamsList())
    {
        static const int ccIndexTbl[7] =
        {
            0, // CC_CC1
            1, // CC_CC2
            9, // sentinel
            9, // sentinel
            2, // CC_CC3
            3, // CC_CC4
            9, // sentinel
        };

        CC608Reader *subReader = GetCC608Reader(streamId);

        while (true)
        {
            bool changed = false;
            int streamRawIdx = -1;
            CC608Buffer *subtitles = subReader->GetOutputText(
                changed, streamRawIdx);

            if (!changed)
                break;

            const int ccIdx = ccIndexTbl[min(streamRawIdx,6)];

            if (ccIdx >= 4)
                continue;

            GotNew608Subtitle(streamId, ccIdx, subtitles);
        }
    }
}

/**
 * Checks for latest teletext changes.
 */

void MythCCExtractorPlayer::ProcessTTXSubtitles(void)
{
    foreach (uint stream_id, GetTTXStreamsList())
    {
        TeletextExtractorReader *reader = ttxReader.value(stream_id, NULL);

        if (reader && !reader->GetUpdatedPages().isEmpty())
        {
            typedef QPair<int, int> qpii;
            foreach (qpii subpageIdc, reader->GetUpdatedPages())
            {
                reader->SetPage(subpageIdc.first, subpageIdc.second);
                TeletextSubPage *subPage = reader->FindSubPage();

                if (subPage)
                {
                    subPage->pagenum = subpageIdc.first; //just in case
                    GotNewTTXPage(stream_id, *subPage);
                }
            }
        }

        if (reader)
            reader->ClearUpdatedPages();
    }
}

/**
 * Process last cc608 changes (last in video).
 * Call it after video will be finished.
 */

void MythCCExtractorPlayer::Finish608SubtitlesProcess(void)
{
    foreach (uint stream_id, GetCC608StreamsList())
    {
        CC608Reader *reader = cc608.value(stream_id, NULL);

        if (reader)
        {
            foreach (int ccIdx, GetCC608Subtitles().keys())
            {
                CC608Buffer subtitles;
                GotNew608Subtitle(stream_id, ccIdx, &subtitles);
            }
        }
    }
}

/**
 * Process last teletext changes (last in video).
 * Call it after video will be finished.
 */

void MythCCExtractorPlayer::FinishTTXSubtitlesProcess(void)
{
    foreach (uint stream_id, GetTTXStreamsList())
    {
        TeletextExtractorReader *reader = ttxReader.value(stream_id, NULL);

        if (reader)
        {
            foreach (int page, GetTTXSubtitles().keys())
            {
                TeletextSubPage subPage;
                memset(&subPage, 0, sizeof(subPage));
                subPage.pagenum = page;
                GotNewTTXPage(stream_id, subPage);
            }
        }
    }
}

/**
 * Adds new subtitle, finishes last if needed.
 * @param content Text content of new subtitle (may be empty).
 * @param list Queue of subtitles we modify.
 */

void MythCCExtractorPlayer::ProcessNewSubtitle(QStringList content,
                                               QList<OneSubtitle> &list)
{
    OneSubtitle last_one = list.isEmpty() ? OneSubtitle() : list.back();

    bool update_last = !list.isEmpty()
        && m_curTime == last_one.start_time
        && !content.isEmpty();

    if (update_last)
    {
        //update text only (need for cc608)
        list.back().text = content;

    }
    else if (content != last_one.text || last_one.length >= 0)
    {
        // Finish previous subtitle.
        if (!last_one.text.isEmpty() && last_one.length < 0)
        {
            list.back().length = m_curTime - last_one.start_time;
        }

        // Put new one if it isn't empty.
        if (!content.isEmpty())
        {
            OneSubtitle new_one;
            new_one.start_time = m_curTime;
            new_one.text = content;

            list.push_back(new_one);
        }
    }
}

/**
 * Adds new subtitle, finishes last if needed.
 * This is a version for DVB graphic subtitles only.
 * @param content Content of the new subtitle (may be empty).
 * We're going to use it's img & start_time fields.
 * @param list Queue of subtitles we modify.
 */

void MythCCExtractorPlayer::ProcessNewSubtitle(OneSubtitle content,
                                               QList<OneSubtitle> &list)
{
    OneSubtitle last_one = list.isEmpty() ? OneSubtitle() : list.back();

    bool update_last = !list.isEmpty() &&
        content.start_time == last_one.start_time &&
        !content.img.isNull();

    if (update_last)
    {
        //update image only
        list.back().img = content.img;
    }
    else if (content.img != last_one.img || last_one.length >= 0)
    {
        // Finish previous subtitle.
        if (!last_one.img.isNull() && last_one.length < 0)
        {
            list.back().length = content.start_time - last_one.start_time;
        }

        // Put new one if it isn't empty.
        if (!content.img.isNull())
        {
            OneSubtitle new_one;
            new_one.start_time = content.start_time;
            new_one.img = content.img;

            list.push_back(new_one);
        }
    }
}

/**
 * Puts new page to appropriate collection, organizes stream of pages to
 * the stream of subtitles in ttxSubtitles.
 * Note: be sure that subPage.pagenum is set properly.
 *
 * @param stream_id Id of teletext stream where subPage from.
 * @param subPage Subpage of teletext we got.
 */

void MythCCExtractorPlayer::GotNewTTXPage(
    uint stream_id, const TeletextSubPage &subPage)
{
    if (!subPage.subtitle)
        return;

    QStringList content = ConvertTTXPage(subPage);
    QList<OneSubtitle> &list = ttxSubtitles[stream_id][subPage.pagenum];
    ProcessNewSubtitle(content, list);
}

/**
 * Extracts text subtitles from the teletext page.
 */

QStringList MythCCExtractorPlayer::ConvertTTXPage(
    const TeletextSubPage &subPage)
{
    QStringList content;

    for (int i = 0; i < 25; ++i)
    {
        QString str = decode_teletext(subPage.lang, subPage.data[i]);
        str = str.trimmed();

        if (!str.isEmpty())
        {
            content << str;
        }
    }

    return content;
}

/**
 * Processes new subtitles if any from all CC708 streams.
 */

void MythCCExtractorPlayer::Process708Subtitles(void)
{
    foreach (uint streamId, GetCC708StreamsList())
    {
        CC708Reader *subReader = GetCC708Reader(streamId);

        for (int serviceIdx = 1; serviceIdx < 64; ++serviceIdx)
        {
            CC708Service *service = subReader->GetService(serviceIdx);
            for (int windowIdx = 0; windowIdx < 8; ++windowIdx)
            {
                CC708Window &win = service->windows[windowIdx];
                if (win.changed && win.visible)
                {
                    vector<CC708String *> strings = win.GetStrings();

                    GotNew708Subtitle(streamId, serviceIdx, windowIdx, strings);

                    service->windows[windowIdx].changed = false;
                }
            }
        }
    }
}

/**
 * Finishes pending CC608 subtitles.
 */

void MythCCExtractorPlayer::Finish708SubtitlesProcess(void)
{
    foreach (uint streamId, GetCC708StreamsList())
    {
        //CC708Reader *subReader = GetCC708Reader(streamId);

        for (int serviceIdx = 1; serviceIdx < 64; ++serviceIdx)
        {
            //CC708Service *service = subReader->GetService(serviceIdx);
            for (int windowIdx = 0; windowIdx < 8; ++windowIdx)
            {
                //if (service->windows[windowIdx].changed)
                {
                    vector<CC708String *> strings; // Empty content.
                    GotNew708Subtitle(streamId, serviceIdx, windowIdx, strings);
                }
            }
        }
    }
}

/**
 * Processes new CC708 subtitle.
 */

void MythCCExtractorPlayer::GotNew708Subtitle(
    uint streamId, int serviceIdx,
    int windowIdx, const vector<CC708String *> &content)
{
    bool empty = true;
    QStringList myContent;
    foreach (CC708String *str, content)
    {
        empty = empty && str->str.trimmed().isEmpty();

        QString t = str->str.trimmed();
        if (!t.isEmpty())
            myContent << t;
    }

    cc708windows[qMakePair(streamId, serviceIdx)][windowIdx] = myContent;
    QStringList myContent2;
    foreach (QStringList list, cc708windows[qMakePair(streamId, serviceIdx)])
    {
        myContent2.append(list);
    }

    QList<OneSubtitle> &list = cc708Subtitles[streamId][serviceIdx];

    ProcessNewSubtitle(myContent2, list);
}

/**
 * Processes new subtitles if any from all dbv streams.
 */

void MythCCExtractorPlayer::ProcessDVBSubtitles(void)
{
    foreach (uint streamId, GetSubtitleStreamsList())
    {
        SubtitleReader *subReader = GetSubReader(streamId);
        OneSubtitle cur_sub;

        if (subReader->HasTextSubtitles())
        {
            LOG(VB_VBI, LOG_DEBUG,
                "There are unhandled text dvb subtitles");
        }

        uint64_t duration;
        const QStringList rawSubs = subReader->GetRawTextSubtitles(duration);
        if (!rawSubs.isEmpty())
        {
            LOG(VB_VBI, LOG_DEBUG,
                QString("There are also %1 raw text subtitles with duration %2")
                .arg(rawSubs.size()).arg(duration));
        }

        AVSubtitles *subtitles = subReader->GetAVSubtitles();
        foreach (const AVSubtitle &buf, subtitles->buffers)
        {
            const QSize v_size = GetVideoSize();
            QImage sub_pict(v_size, QImage::Format_ARGB32);
            sub_pict.fill(0);

            int min_x = v_size.width();
            int min_y = v_size.height();
            int max_x = 0;
            int max_y = 0;

            QPainter painter(&sub_pict);

            for (int i = 0; i < (int) buf.num_rects; ++i)
            {
                AVSubtitleRect *rect = buf.rects[i];

                if (buf.rects[i]->type == SUBTITLE_BITMAP)
                {
                    const int x = rect->x;
                    const int y = rect->y;
                    const int w = rect->w;
                    const int h = rect->h;
                    const int cc = rect->nb_colors;
                    const uchar *data = rect->pict.data[0];
                    const QRgb *palette = (QRgb *) rect->pict.data[1];

                    QImage img(data, w, h, QImage::Format_Indexed8);
                    img.setColorCount(cc);
                    for (int i = 0; i < cc; ++i)
                    {
                        img.setColor(i, palette[i]);
                    }

                    painter.drawImage(x, y, img);

                    min_x = min(min_x, x);
                    min_y = min(min_y, y);
                    max_x = max(max_x, x + w);
                    max_y = max(max_y, y + h);
                }
            }

            OneSubtitle sub;
            sub.start_time = buf.start_display_time - m_curTimeShift;
            sub.length = buf.end_display_time - buf.start_display_time;

            if (min_x < max_x && min_y < max_y)
            {
                sub.img_shift = QPoint(min_x, min_y);
                sub.img = sub_pict.copy(min_x, min_y, max_x - min_x, max_y - min_y);

            }
            else
            {
                // Empty subtitle, do nothing.
            }

            //?
            cur_sub = sub;
            break;
        }

//        if (!cur_sub.img.isNull() || !cur_sub.text.isEmpty()) {
//            dvbSubtitles[streamId].append(cur_sub);
//        }

            QList<OneSubtitle> &list = dvbSubtitles[streamId];
            ProcessNewSubtitle(cur_sub, list);

            subReader->ClearAVSubtitles();
            subReader->ClearRawTextSubtitles();
        }
    }

/**
 * Finishes pending DVB subtitles.
 */

void MythCCExtractorPlayer::FinishDVBSubtitlesProcess(void)
{
    // Do nothing.
}

CC708Reader *MythCCExtractorPlayer::GetCC708Reader(uint id)
{
    QHash<uint, CC708Reader*>::iterator it = cc708.find(id);
    if (it != cc708.end())
        return *it;

    //printf("Make CC708Reader for %u\n", id);

    // TODO FIXME ? safe to pass "this" ?
    CC708Reader *n = new CC708Reader(this);
    n->SetEnabled(true);

    cc708.insert(id, n);
    return n;
}

CC608Reader *MythCCExtractorPlayer::GetCC608Reader(uint id)
{
    QHash<uint, CC608Reader*>::iterator it = cc608.find(id);
    if (it != cc608.end())
        return *it;

    //printf("Make CC608Reader for %u\n", id);

    // TODO FIXME ? safe to pass "this" ?
    CC608Reader *n = new CC608Reader(this);
    n->SetEnabled(true);

    cc608.insert(id, n);
    return n;
}

SubtitleReader *MythCCExtractorPlayer::GetSubReader(uint id)
{
    QHash<uint, SubtitleReader*>::iterator it = subReader.find(id);
    if (it != subReader.end())
        return *it;

    //printf("Make SubReader for %u\n", id);

    SubtitleReader *n = new SubtitleReader();

    n->EnableAVSubtitles(true);
    n->EnableTextSubtitles(true);
    n->EnableRawTextSubtitles(true);

    subReader.insert(id, n);
    return n;
}

TeletextReader *MythCCExtractorPlayer::GetTeletextReader(uint id)
{
    QHash<uint, TeletextExtractorReader *>::iterator it = ttxReader.find(id);
    if (it != ttxReader.end())
        return *it;

    //printf("Make TeletextReader for %u\n", id);

    TeletextExtractorReader *n = new TeletextExtractorReader();
    ttxReader.insert(id, n);
    return n;
}
