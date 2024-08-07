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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <iostream>
#include <utility>

#include <QFileInfo>
#include <QPainter>

#include "libmythbase/iso639.h"

#include "captions/srtwriter.h"
#include "captions/subtitlescreen.h"
#include "captions/teletextextractorreader.h"
#include "decoders/avformatdecoder.h"
#include "mythccextractorplayer.h"

SRTStuff::~SRTStuff()
{
    while (!m_srtWriters.empty())
    {
        delete *m_srtWriters.begin();
        m_srtWriters.erase(m_srtWriters.begin());
    }
}
CC608Stuff::~CC608Stuff() { delete m_reader; }
CC708Stuff::~CC708Stuff() { delete m_reader; }
TeletextStuff::~TeletextStuff() { delete m_reader; }
DVBSubStuff::~DVBSubStuff() { delete m_reader; }

MythCCExtractorPlayer::MythCCExtractorPlayer(PlayerContext *Context, PlayerFlags flags, bool showProgress,
                                             QString fileName,
                                             const QString &destdir) :
    MythPlayer(Context, flags),
    m_showProgress(showProgress),
    m_fileName(std::move(fileName))
{
    // Determine where we will put extracted info.
    QStringList comps = QFileInfo(m_fileName).fileName().split(".");
    if (!comps.empty())
        comps.removeLast();
    if (destdir.isEmpty())
        m_workingDir = QDir(QFileInfo(m_fileName).path());
    else
    {
        m_workingDir = QDir(destdir);
        if (!m_workingDir.exists())
            m_workingDir = QDir(QFileInfo(m_fileName).path());
    }
    m_baseName = comps.join(".");
}

/**
 * Call it when you got new video frame to process subtitles if any.
 */

void MythCCExtractorPlayer::OnGotNewFrame(void)
{
    m_myFramesPlayed = m_decoder->GetFramesRead();
    m_videoOutput->StartDisplayingFrame();
    {
        MythVideoFrame *frame = m_videoOutput->GetLastShownFrame();
        double fps = frame->m_frameRate;
        if (fps <= 0)
            fps = GetDecoder()->GetFPS();
        double duration = (1 / fps) + (static_cast<double>(frame->m_repeatPic) * 0.5 / fps);
        m_curTime += secondsFromFloat(duration);
        m_videoOutput->DoneDisplayingFrame(frame);
    }

    Ingest608Captions();  Process608Captions(kProcessNormal);
    Ingest708Captions();  Process708Captions(kProcessNormal);
    IngestTeletext();     ProcessTeletext(kProcessNormal);
    IngestDVBSubtitles(); ProcessDVBSubtitles(kProcessNormal);
}

static QString progress_string(
    MythTimer &flagTime, uint64_t m_myFramesPlayed, uint64_t totalFrames)
{
    if (totalFrames == 0ULL)
    {
        return QString("%1 frames processed    \r")
            .arg(m_myFramesPlayed,7);
    }

    static const std::string kSpinChars { R"(/-\|)" };
    static uint s_spinCnt = 0;

    double elapsed = flagTime.elapsed().count() * 0.001;
    double flagFPS = (elapsed > 0.0) ? (m_myFramesPlayed / elapsed) : 0;

    double percentage = m_myFramesPlayed * 100.0 / totalFrames;
    percentage = (percentage > 100.0 && percentage < 101.0) ?
        100.0 : percentage;

    if (m_myFramesPlayed < totalFrames)
        return QString("%1 fps %2%      \r")
          .arg(flagFPS,4,'f', (flagFPS < 10.0 ? 1 : 0)).arg(percentage,4,'f',1);
    return QString("%1 fps %2      \r")
        .arg(flagFPS,4,'f', (flagFPS < 10.0 ? 1 : 0))
        .arg(kSpinChars[++s_spinCnt % 4]);
}

bool MythCCExtractorPlayer::run(void)
{
    m_myFramesPlayed = 0;

    m_killDecoder = false;
    m_framesPlayed = 0;

    m_decoder->SetDecodeAllSubtitles(true);

    SetPlaying(true);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to initialize video");
        SetPlaying(false);
        return false;
    }

    ClearAfterSeek();

    MythTimer flagTime;
    MythTimer ui_timer;
    MythTimer inuse_timer;
    MythTimer save_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();
    save_timer.start();

    m_curTime = 0ms;

    if (DecoderGetFrame(kDecodeVideo))
        OnGotNewFrame();

    if (m_showProgress)
        std::cout << "\r                                      \r" << std::flush;

    while (!m_killDecoder && !IsErrored())
    {
        if (inuse_timer.elapsed() > 2534ms)
        {
            inuse_timer.restart();
            m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
            if (m_playerCtx->m_playingInfo)
                m_playerCtx->m_playingInfo->UpdateInUseMark();
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (m_showProgress && (ui_timer.elapsed() > 98ms * 4))
        {
            ui_timer.restart();
            QString str = progress_string(
                flagTime, m_myFramesPlayed, m_totalFrames);
            std::cout << qPrintable(str) << '\r' << std::flush;
        }

        if (!DecoderGetFrame(kDecodeVideo))
            break;

        OnGotNewFrame();
    }

    if (m_showProgress)
    {
        if ((m_myFramesPlayed < m_totalFrames) &&
            ((m_myFramesPlayed + 30) > m_totalFrames))
        {
            m_myFramesPlayed = m_totalFrames;
        }
        QString str = progress_string(flagTime, m_myFramesPlayed, m_totalFrames);
        std::cout << qPrintable(str) << std::endl;
    }

    Process608Captions(kProcessFinalize);
    Process708Captions(kProcessFinalize);
    ProcessTeletext(kProcessFinalize);
    ProcessDVBSubtitles(kProcessFinalize);

    SetPlaying(false);
    m_killDecoder = true;

    return true;
}


/**
 * Adds new subtitle, finishes last if needed.
 * @param content Text content of new subtitle (may be empty).
 * @param list Queue of subtitles we modify.
 */

void MythCCExtractorPlayer::IngestSubtitle(
    QList<OneSubtitle> &list, const QStringList &content) const
{
    bool update_last =
        !list.isEmpty() &&
        m_curTime == list.back().m_startTime &&
        !content.isEmpty();

    if (update_last)
    {
        //update text only (need for cc608)
        list.back().m_text = content;
        return;
    }

    OneSubtitle last_one = list.isEmpty() ? OneSubtitle() : list.back();
    if (content != last_one.m_text || last_one.m_length >= 0ms)
    {
        // Finish previous subtitle.
        if (!last_one.m_text.isEmpty() && last_one.m_length < 0ms)
        {
            list.back().m_length = m_curTime - last_one.m_startTime;
        }

        // Put new one if it isn't empty.
        if (!content.isEmpty())
        {
            OneSubtitle new_one;
            new_one.m_startTime = m_curTime;
            new_one.m_text = content;

            list.push_back(new_one);
        }
    }
}

/**
 * Adds new subtitle, finishes last if needed.
 * This is a version for DVB graphical subtitles only.
 * @param list Queue of subtitles we modify.
 * @param content Content of the new subtitle (may be empty).
 * We're going to use it's m_img & m_startTime fields.
 */
void MythCCExtractorPlayer::IngestSubtitle(
    QList<OneSubtitle> &list, const OneSubtitle &content)
{
    bool update_last =
        !list.isEmpty() &&
        content.m_startTime == list.back().m_startTime &&
        !content.m_img.isNull();

    if (update_last)
    {
        list.back().m_img = content.m_img; // update image only
        return;
    }

    OneSubtitle last_one = list.isEmpty() ? OneSubtitle() : list.back();
    if (content.m_img != last_one.m_img || last_one.m_length >= 0ms)
    {
        // Finish previous subtitle.
        if (!last_one.m_img.isNull() && last_one.m_length < 0ms)
        {
            list.back().m_length = content.m_startTime - last_one.m_startTime;
        }

        // Put new one if it isn't empty.
        if (!content.m_img.isNull())
        {
            OneSubtitle new_one;
            new_one.m_startTime = content.m_startTime;
            new_one.m_img = content.m_img;

            list.push_back(new_one);
        }
    }
}

void MythCCExtractorPlayer::Ingest608Captions(void)
{
    static constexpr std::array<int,7> kCcIndexTbl
    {
        0, // CC_CC1
        1, // CC_CC2
        9, // sentinel
        9, // sentinel
        2, // CC_CC3
        3, // CC_CC4
        9, // sentinel
    };

    // for each CC of each video...
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_cc608Info.begin(); it != m_cc608Info.end(); ++it)
    {
        while (true)
        {
            bool changed = false;
            int streamRawIdx = -1;
            CC608Buffer *textlist = (*it).m_reader->GetOutputText(
                changed, streamRawIdx);

            if (!changed || !textlist)
                break;

            if (streamRawIdx < 0)
                continue;

            textlist->m_lock.lock();

            const int ccIdx = kCcIndexTbl[std::min(streamRawIdx,6)];

            if (ccIdx >= 4)
            {
                textlist->m_lock.unlock();
                continue;
            }

            FormattedTextSubtitle608 fsub(textlist->m_buffers);
            QStringList content = fsub.ToSRT();

            textlist->m_lock.unlock();

            IngestSubtitle((*it).m_subs[ccIdx], content);
        }
    }
}

// Note: GetCaptionLanguage() will not return valid if there are multiple videos
void MythCCExtractorPlayer::Process608Captions(uint flags)
{
    int i = 0;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto cc608it = m_cc608Info.begin(); cc608it != m_cc608Info.end(); ++cc608it)
    {
        QString stream_id_str = (m_cc608Info.size() <= 1) ?
            QString("") : QString("%1.").arg(i,2,10,QChar('0'));

        CC608StreamType &subs = (*cc608it).m_subs;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            if ((*it).empty())
                continue; // Skip empty subtitle streams.
            if (((kProcessFinalize & flags) == 0) && ((*it).size() <= 1))
                continue; // Leave one caption behind so it can be amended

            int idx = it.key();

            if (!(*cc608it).m_srtWriters[idx])
            {
                int langCode = 0;
                auto *avd = dynamic_cast<AvFormatDecoder *>(m_decoder);
                if (avd)
                    langCode = avd->GetCaptionLanguage(
                        kTrackTypeCC608, idx + 1);

                QString lang = iso639_key_to_str3(langCode);
                lang = iso639_is_key_undefined(langCode) ? "und" : lang;

                QString service_key = QString("cc%1").arg(idx + 1);
                QString filename = QString("%1.%2%3-%4.%5.srt")
                    .arg(m_baseName, stream_id_str, "608",
                         service_key, lang);

                (*cc608it).m_srtWriters[idx] = new SRTWriter(
                    m_workingDir.filePath(filename));
            }

            if (!(*cc608it).m_srtWriters[idx]->IsOpen())
            {
                (*it).clear();
                continue;
            }

            while ((*it).size() > ((kProcessFinalize & flags) ? 0 : 1))
            {
                if ((*it).front().m_length <= 0ms)
                    (*it).front().m_length = OneSubtitle::kDefaultLength;

                (*cc608it).m_srtWriters[idx]->AddSubtitle(
                    (*it).front(), ++(*cc608it).m_subsNum[idx]);
                (*it).pop_front();
            }

            (*cc608it).m_srtWriters[idx]->Flush();
        }
    }
}

void MythCCExtractorPlayer::Ingest708Captions(void)
{
    // For each window of each service of each video...
    for (auto it = m_cc708Info.cbegin(); it != m_cc708Info.cend(); ++it)
    {
        for (uint serviceIdx = 1; serviceIdx < k708MaxServices; ++serviceIdx)
        {
            CC708Service *service = (*it).m_reader->GetService(serviceIdx);
            for (uint windowIdx = 0; windowIdx < 8; ++windowIdx)
            {
                CC708Window &win = service->m_windows[windowIdx];
                if (win.GetChanged())
                {
                    std::vector<CC708String*> strings;
                    if (win.GetVisible())
                    {
                        strings = win.GetStrings();
                        Ingest708Caption(it.key(), serviceIdx, windowIdx,
                                         win.m_pen.m_row, win.m_pen.m_column,
                                         win, strings);
                        CC708Window::DisposeStrings(strings);
                    }
                    service->m_windows[windowIdx].ResetChanged();
                }
            }
        }
    }
}

void MythCCExtractorPlayer::Ingest708Caption(
    uint streamId, uint serviceIdx,
    uint windowIdx, uint start_row, uint start_column,
    const CC708Window &win,
    const std::vector<CC708String*> &content)
{
    FormattedTextSubtitle708 fsub(win, windowIdx, content);
    QStringList winContent = fsub.ToSRT();

    QMap<int, Window> &cc708win = m_cc708Windows[streamId][serviceIdx];
    cc708win[windowIdx].row = start_row;
    cc708win[windowIdx].column = start_column;
    cc708win[windowIdx].text = winContent;

    QMap<uint, QStringList> orderedContent;
    for (const auto& ccIt : std::as_const(cc708win))
    {
        uint idx = (ccIt.row * 1000) + ccIt.column;
        for (const auto& str : std::as_const(ccIt.text))
        {
            orderedContent[idx] += str;
        }
    }

    QStringList screenContent;
    for (const auto & ordered : std::as_const(orderedContent))
        screenContent += ordered;
    IngestSubtitle(m_cc708Info[streamId].m_subs[serviceIdx], screenContent);
}

// Note: GetCaptionLanguage() will not return valid if there are multiple videos
void MythCCExtractorPlayer::Process708Captions(uint flags)
{
    int i = 0;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto cc708it = m_cc708Info.begin(); cc708it != m_cc708Info.end(); ++cc708it)
    {
        QString stream_id_str = (m_cc708Info.size() <= 1) ?
            QString("") : QString("%1.").arg(i,2,10,QChar('0'));

        CC708StreamType &subs = (*cc708it).m_subs;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            if ((*it).empty())
                continue; // Skip empty subtitle streams.
            if (((kProcessFinalize & flags) == 0) && ((*it).size() <= 1))
                continue; // Leave one caption behind so it can be amended

            int idx = it.key();

            if (!(*cc708it).m_srtWriters[idx])
            {
                int langCode = 0;
                auto *avd = dynamic_cast<AvFormatDecoder*>(m_decoder);
                if (avd)
                    langCode = avd->GetCaptionLanguage(kTrackTypeCC708, idx);

                QString lang = iso639_key_to_str3(langCode);

                QString service_key = QString("service-%1")
                    .arg(idx, 2, 10, QChar('0'));
                QString filename = QString("%1.%2%3-%4.%5.srt")
                    .arg(m_baseName, stream_id_str, "708",
                         service_key, lang);

                (*cc708it).m_srtWriters[idx] = new SRTWriter(
                    m_workingDir.filePath(filename));
            }

            if (!(*cc708it).m_srtWriters[idx]->IsOpen())
            {
                (*it).clear();
                continue;
            }

            while ((*it).size() > ((kProcessFinalize & flags) ? 0 : 1))
            {
                if ((*it).front().m_length <= 0ms)
                    (*it).front().m_length = OneSubtitle::kDefaultLength;

                (*cc708it).m_srtWriters[idx]->AddSubtitle(
                    (*it).front(), ++(*cc708it).m_subsNum[idx]);
                (*it).pop_front();
            }

            (*cc708it).m_srtWriters[idx]->Flush();
        }
    }
}

static QStringList to_string_list(const TeletextSubPage &subPage)
{
    QStringList content;
    // Skip the page header (line 0)
    for (size_t i = 1; i < subPage.data.size(); ++i)
    {
        QString str = decode_teletext(subPage.lang, subPage.data[i]).trimmed();
        if (!str.isEmpty())
            content += str;
    }
    return content;
}

void MythCCExtractorPlayer::IngestTeletext(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto ttxit = m_ttxInfo.begin(); ttxit != m_ttxInfo.end(); ++ttxit)
    {
        using qpii = QPair<int, int>;
        QSet<qpii> updatedPages = (*ttxit).m_reader->GetUpdatedPages();
        if (updatedPages.isEmpty())
            continue;

        for (auto it = updatedPages.constBegin(); it != updatedPages.constEnd(); ++it)
        {
            (*ttxit).m_reader->SetPage((*it).first, (*it).second);
            TeletextSubPage *subpage = (*ttxit).m_reader->FindSubPage();
            if (subpage && subpage->subtitle)
            {
                IngestSubtitle((*ttxit).m_subs[(*it).first],
                               to_string_list(*subpage));
            }
        }

        (*ttxit).m_reader->ClearUpdatedPages();
    }
}

void MythCCExtractorPlayer::ProcessTeletext(uint flags)
{
    int i = 0;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto ttxit = m_ttxInfo.begin(); ttxit != m_ttxInfo.end(); ++ttxit)
    {
        QString stream_id_str = (m_cc608Info.size() <= 1) ?
            QString("") : QString("%1.").arg(i,2,10,QChar('0'));

        TeletextStreamType &subs = (*ttxit).m_subs;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            if ((*it).empty())
                continue; // Skip empty subtitle streams.
            if (((kProcessFinalize & flags) == 0) && ((*it).size() <= 1))
                continue; // Leave one caption behind so it can be amended

            uint page = it.key();

            if (!(*ttxit).m_srtWriters[page])
            {
                int langCode = 0;
                auto *avd = dynamic_cast<AvFormatDecoder *>(m_decoder);

                if (avd)
                    langCode = avd->GetTeletextLanguage(page);

                QString lang = iso639_key_to_str3(langCode);
                lang = iso639_is_key_undefined(langCode) ? "und" : lang;
                QString filename = QString("%1-%2.%3ttx-0x%4.srt")
                    .arg(m_baseName, lang, stream_id_str)
                    .arg(page, 3, 16, QChar('0'));

                (*ttxit).m_srtWriters[page] = new SRTWriter(
                    m_workingDir.filePath(filename));
            }

            if (!(*ttxit).m_srtWriters[page]->IsOpen())
            {
                (*it).clear();
                continue;
            }

            while ((*it).size() > ((kProcessFinalize & flags) ? 0 : 1))
            {
                if ((*it).front().m_length <= 0ms)
                    (*it).front().m_length = OneSubtitle::kDefaultLength;

                (*ttxit).m_srtWriters[page]->AddSubtitle(
                    (*it).front(), ++(*ttxit).m_subsNum[page]);
                (*it).pop_front();
            }

            (*ttxit).m_srtWriters[page]->Flush();
        }
    }
}

void MythCCExtractorPlayer::IngestDVBSubtitles(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto subit = m_dvbsubInfo.begin(); subit != m_dvbsubInfo.end(); ++subit)
    {
        /// INFO -- start
        if ((*subit).m_reader->HasTextSubtitles())
        {
            LOG(VB_VBI, LOG_DEBUG,
                "There are unhandled text dvb subtitles");
        }

        std::chrono::milliseconds duration = 0ms;
        const QStringList rawSubs =
            (*subit).m_reader->GetRawTextSubtitles(duration);
        if (!rawSubs.isEmpty())
        {
            LOG(VB_VBI, LOG_DEBUG,
                QString("There are also %1 raw text subtitles with duration %2")
                .arg(rawSubs.size()).arg(duration.count()));
        }
        /// INFO -- end

        AVSubtitles *avsubtitles = (*subit).m_reader->GetAVSubtitles();

        QMutexLocker locker(&(avsubtitles->m_lock));

        while (!avsubtitles->m_buffers.empty())
        {
            AVSubtitle subtitle = avsubtitles->m_buffers.front();
            avsubtitles->m_buffers.pop_front();

            const QSize v_size =
                QSize(GetVideoSize().width()*4, GetVideoSize().height()*4);
            QImage sub_pict(v_size, QImage::Format_ARGB32);
            sub_pict.fill(0);

            int min_x = v_size.width();
            int min_y = v_size.height();
            int max_x = 0;
            int max_y = 0;

            QPainter painter(&sub_pict);
            for (int i = 0; i < (int) subtitle.num_rects; ++i)
            {
                AVSubtitleRect *rect = subtitle.rects[i];

                if (subtitle.rects[i]->type == SUBTITLE_BITMAP)
                {
                    const int x = rect->x;
                    const int y = rect->y;
                    const int w = rect->w;
                    const int h = rect->h;
                    const int cc = rect->nb_colors;
                    const uchar *data = rect->data[0];
                    const QRgb *palette = (QRgb *) rect->data[1];

                    QImage img(data, w, h, QImage::Format_Indexed8);
                    img.setColorCount(cc);
                    for (int j = 0; j < cc; ++j)
                        img.setColor(j, palette[j]);

                    painter.drawImage(x, y, img);

                    min_x = std::min(min_x, x);
                    min_y = std::min(min_y, y);
                    max_x = std::max(max_x, x + w);
                    max_y = std::max(max_y, y + h);
                }
            }
            painter.end();

            OneSubtitle sub;
            sub.m_startTime =
                std::chrono::milliseconds(subtitle.start_display_time);
            sub.m_length =
                std::chrono::milliseconds(subtitle.end_display_time -
                                          subtitle.start_display_time);

            SubtitleReader::FreeAVSubtitle(subtitle);

            if (min_x < max_x && min_y < max_y)
            {
                sub.m_imgShift = QPoint(min_x, min_y);
                sub.m_img = sub_pict.copy(
                    min_x, min_y, max_x - min_x, max_y - min_y);
            }
            else
            {
                // Empty subtitle, do nothing.
            }

            IngestSubtitle((*subit).m_subs, sub);
        }

        locker.unlock();

        (*subit).m_reader->ClearRawTextSubtitles();
    }
}

void MythCCExtractorPlayer::ProcessDVBSubtitles(uint flags)
{
    // Process (DVB) subtitle streams.
    int subtitleStreamCount = 0;
    for (auto subit = m_dvbsubInfo.begin(); subit != m_dvbsubInfo.end(); ++subit)
    {
        int langCode = 0;
        auto *avd = dynamic_cast<AvFormatDecoder *>(m_decoder);
        int idx = subit.key();
        if (avd)
            langCode = avd->GetSubtitleLanguage(subtitleStreamCount, idx);
        subtitleStreamCount++;

        QString lang = iso639_key_to_str3(langCode);
        lang = iso639_is_key_undefined(langCode) ? "und" : lang;
        QString dir_name = m_baseName + QString("-%1.dvb-%2").arg(lang).arg(subit.key());
        if (!m_workingDir.exists(dir_name) && !m_workingDir.mkdir(dir_name))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Can't create directory '%1'")
                .arg(dir_name));
            (*subit).m_subs.clear();
            continue;
        }

        DVBStreamType &subs = (*subit).m_subs;
        if (subs.empty())
            continue; // Skip empty subtitle streams.
        if (((kProcessFinalize & flags) == 0) && (subs.size() <= 1))
            continue; // Leave one caption behind so it can be amended

        QDir stream_dir(m_workingDir.filePath(dir_name));
        while (subs.size() > ((kProcessFinalize & flags) ? 0 : 1))
        {
            if (subs.front().m_length <= 0ms)
                subs.front().m_length = OneSubtitle::kDefaultLength;

            const OneSubtitle &sub = subs.front();
            std::chrono::milliseconds end_time = sub.m_startTime + sub.m_length;
            const QString file_name =
                stream_dir.filePath(
                    QString("%1_%2-to-%3.png")
                    .arg((*subit).m_subsNum)
                    .arg(sub.m_startTime.count()).arg(end_time.count()));

            if (end_time > sub.m_startTime)
            {
                //check is there exist file with same m_startTime
                QStringList filter;
                filter << QString("*_%1*.png").arg(sub.m_startTime.count());
                QFileInfoList found = stream_dir.entryInfoList(filter);
                if (found.isEmpty())
                {
                    //no same m_startTime founded
                    if (!sub.m_img.save(file_name))
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("Can't write file '%1'")
                            .arg(file_name));
                    }
                    (*subit).m_subsNum++;
                }
            }
            subs.pop_front();
        }
    }
}


CC708Reader *MythCCExtractorPlayer::GetCC708Reader(uint id)
{
    if (!m_cc708Info[id].m_reader)
    {
        m_cc708Info[id].m_reader = new CC708Reader(this);
        m_cc708Info[id].m_reader->SetEnabled(true);
        LOG(VB_GENERAL, LOG_INFO, "Created CC708Reader");
    }
    return m_cc708Info[id].m_reader;
}

CC608Reader *MythCCExtractorPlayer::GetCC608Reader(uint id)
{
    if (!m_cc608Info[id].m_reader)
    {
        m_cc608Info[id].m_reader = new CC608Reader(this);
        m_cc608Info[id].m_reader->SetEnabled(true);
    }
    return m_cc608Info[id].m_reader;
}

TeletextReader *MythCCExtractorPlayer::GetTeletextReader(uint id)
{
    if (!m_ttxInfo[id].m_reader)
        m_ttxInfo[id].m_reader = new TeletextExtractorReader();
    return m_ttxInfo[id].m_reader;
}

SubtitleReader *MythCCExtractorPlayer::GetSubReader(uint id)
{
    if (!m_dvbsubInfo[id].m_reader)
    {
        m_dvbsubInfo[id].m_reader = new SubtitleReader(this);
        m_dvbsubInfo[id].m_reader->EnableAVSubtitles(true);
        m_dvbsubInfo[id].m_reader->EnableTextSubtitles(true);
        m_dvbsubInfo[id].m_reader->EnableRawTextSubtitles(true);
    }
    return m_dvbsubInfo[id].m_reader;
}
