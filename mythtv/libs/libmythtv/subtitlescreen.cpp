#include <QFontMetrics>

#include "mythverbose.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythuishape.h"
#include "mythuiimage.h"
#include "mythpainter.h"
#include "subtitlescreen.h"

#define LOC      QString("Subtitles: ")
#define LOC_WARN QString("Subtitles Warning: ")
static MythFontProperties* gTextSubFont;
static QHash<int, MythFontProperties*> gCC708Fonts;

SubtitleScreen::SubtitleScreen(MythPlayer *player, const char * name) :
    MythScreenType((MythScreenType*)NULL, name),
    m_player(player),  m_subreader(NULL),   m_608reader(NULL),
    m_708reader(NULL), m_safeArea(QRect()), m_useBackground(false),
    m_removeHTML(QRegExp("</?.+>")),        m_subtitleType(kDisplayNone),
    m_708fontZoom(100),                     m_refreshArea(false)
{
    m_708fontSizes[0] = 36;
    m_708fontSizes[1] = 45;
    m_708fontSizes[2] = 60;
    m_removeHTML.setMinimal(true);
}

SubtitleScreen::~SubtitleScreen(void)
{
    ClearAllSubtitles();
}

void SubtitleScreen::EnableSubtitles(int type)
{
    m_subtitleType = type;
    if (m_subreader)
    {
        m_subreader->EnableAVSubtitles(kDisplayAVSubtitle == m_subtitleType);
        m_subreader->EnableTextSubtitles(kDisplayTextSubtitle == m_subtitleType);
    }
    if (m_608reader)
        m_608reader->SetEnabled(kDisplayCC608 == m_subtitleType);
    if (m_708reader)
        m_708reader->SetEnabled(kDisplayCC708 == m_subtitleType);
    ClearAllSubtitles();
    SetVisible(m_subtitleType != kDisplayNone);
    SetArea(MythRect());
}

bool SubtitleScreen::Create(void)
{
    if (!m_player)
        return false;

    m_subreader = m_player->GetSubReader();
    m_608reader = m_player->GetCC608Reader();
    m_708reader = m_player->GetCC708Reader();
    if (!m_subreader)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to get subtitle reader.");
    if (!m_608reader)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to get CEA-608 reader.");
    if (!m_708reader)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to get CEA-708 reader.");
    m_useBackground = (bool)gCoreContext->GetNumSetting("CCBackground", 0);
    m_708fontZoom   = gCoreContext->GetNumSetting("OSDCC708TextZoom", 100);
    return true;
}

void SubtitleScreen::Pulse(void)
{
    ExpireSubtitles();
    if (kDisplayAVSubtitle == m_subtitleType)
        DisplayAVSubtitles();
    else if (kDisplayTextSubtitle == m_subtitleType)
        DisplayTextSubtitles();
    else if (kDisplayCC608 == m_subtitleType)
        DisplayCC608Subtitles();
    else if (kDisplayCC708 == m_subtitleType)
        DisplayCC708Subtitles();

    OptimiseDisplayedArea();
    m_refreshArea = false;
}

void SubtitleScreen::ClearAllSubtitles(void)
{
    ClearNonDisplayedSubtitles();
    ClearDisplayedSubtitles();
}

void SubtitleScreen::ClearNonDisplayedSubtitles(void)
{
    if (m_subreader && (kDisplayAVSubtitle == m_subtitleType))
        m_subreader->ClearAVSubtitles();
    if (m_608reader && (kDisplayCC608 == m_subtitleType))
        m_608reader->ClearBuffers(true, true);
    if (m_708reader && (kDisplayCC708 == m_subtitleType))
        m_708reader->ClearBuffers();
}

void SubtitleScreen::ClearDisplayedSubtitles(void)
{
    for (int i = 0; i < 8; i++)
        Clear708Cache(i);
    DeleteAllChildren();
    m_expireTimes.clear();
    SetRedraw();
}

void SubtitleScreen::ExpireSubtitles(void)
{
    QDateTime now = QDateTime::currentDateTime();
    QMutableHashIterator<MythUIType*, QDateTime> it(m_expireTimes);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < now)
        {
            DeleteChild(it.key());
            it.remove();
            SetRedraw();
        }
    }
}

void SubtitleScreen::OptimiseDisplayedArea(void)
{
    if (!m_refreshArea)
        return;

    QRegion visible;
    QListIterator<MythUIType *> i(m_ChildrenList);
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        visible = visible.united(img->GetArea());
    }

    if (visible.isEmpty())
        return;

    QRect bounding  = visible.boundingRect();
    bounding = bounding.translated(m_safeArea.topLeft());
    bounding = m_safeArea.intersected(bounding);
    int left = m_safeArea.left() - bounding.left();
    int top  = m_safeArea.top()  - bounding.top();
    SetArea(MythRect(bounding));

    i.toFront();;
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        img->SetArea(img->GetArea().translated(left, top));
    }
}

void SubtitleScreen::DisplayAVSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    VideoOutput    *videoOut = m_player->getVideoOutput();
    VideoFrame *currentFrame = videoOut ? videoOut->GetLastShownFrame() : NULL;

    if (!currentFrame || !videoOut)
        return;

    float tmp = 0.0;
    QRect dummy;
    videoOut->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);

    AVSubtitles* subs = m_subreader->GetAVSubtitles();
    subs->lock.lock();
    while (!subs->buffers.empty())
    {
        const AVSubtitle subtitle = subs->buffers.front();
        if (subtitle.start_display_time > currentFrame->timecode)
            break;

        ClearDisplayedSubtitles();
        subs->buffers.pop_front();
        for (std::size_t i = 0; i < subtitle.num_rects; ++i)
        {
            AVSubtitleRect* rect = subtitle.rects[i];

            bool displaysub = true;
            if (subs->buffers.size() > 0 &&
                subs->buffers.front().end_display_time <
                currentFrame->timecode)
            {
                displaysub = false;
            }

            if (displaysub && rect->type == SUBTITLE_BITMAP)
            {
                // AVSubtitleRect's image data's not guaranteed to be 4 byte
                // aligned.

                QRect qrect(rect->x, rect->y, rect->w, rect->h);
                QRect scaled = videoOut->GetImageRect(qrect);

                QImage qImage(rect->w, rect->h, QImage::Format_ARGB32);
                for (int y = 0; y < rect->h; ++y)
                {
                    for (int x = 0; x < rect->w; ++x)
                    {
                        const uint8_t color = rect->pict.data[0][y*rect->pict.linesize[0] + x];
                        const uint32_t pixel = *((uint32_t*)rect->pict.data[1]+color);
                        qImage.setPixel(x, y, pixel);
                    }
                }

                if (scaled.size() != qrect.size())
                {
                    qImage = qImage.scaled(scaled.width(), scaled.height(),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }

                MythPainter *osd_painter = videoOut->GetOSDPainter();
                MythImage* image = NULL;
                if (osd_painter)
                   image = osd_painter->GetFormatImage();

                long long displayfor = subtitle.end_display_time -
                                       subtitle.start_display_time;
                displayfor = (displayfor < 50) ? 50 : displayfor;
                if (displayfor == 0)
                    displayfor = 60000;
                long long late = currentFrame->timecode -
                                 subtitle.start_display_time;
                MythUIImage *uiimage = NULL;
                if (image)
                {
                    image->Assign(qImage);
                    QString name = QString("avsub%1").arg(i);
                    uiimage = new MythUIImage(this, name);
                    if (uiimage)
                    {
                        m_refreshArea = true;
                        uiimage->SetImage(image);
                        uiimage->SetArea(MythRect(scaled));
                        QDateTime expires =
                            QDateTime::currentDateTime().addMSecs(displayfor);
                        m_expireTimes.insert(uiimage, expires);
                    }
                }
                if (uiimage)
                {
                    VERBOSE(VB_PLAYBACK, LOC +
                        QString("Display AV sub for %1 ms").arg(displayfor));
                    if (late > 50)
                        VERBOSE(VB_PLAYBACK, LOC +
                            QString("AV Sub was %1 ms late").arg(late));
                }
            }
        }
        m_subreader->FreeAVSubtitle(subtitle);
    }
    subs->lock.unlock();
}

void SubtitleScreen::DisplayTextSubtitles(void)
{
    if (!InitialiseFont() || !m_player || !m_subreader)
        return;

    bool changed = false;
    VideoOutput *vo = m_player->getVideoOutput();
    if (vo)
    {
        QRect oldsafe = m_safeArea;
        m_safeArea = m_player->getVideoOutput()->GetSafeRect();
        if (oldsafe != m_safeArea)
        {
            changed = true;
            gTextSubFont->GetFace()->setPixelSize(m_safeArea.height() / 18);
            gTextSubFont->SetColor(Qt::white);
            gTextSubFont->SetOutline(true, Qt::black, 2, 255);
        }
    }
    else
    {
        return;
    }

    VideoFrame *currentFrame = vo->GetLastShownFrame();
    if (!currentFrame)
        return;

    TextSubtitles *subs = m_subreader->GetTextSubtitles();
    subs->Lock();
    uint64_t playPos = 0;
    if (subs->IsFrameBasedTiming())
    {
        // frame based subtitles get out of synch after running mythcommflag
        // for the file, i.e., the following number is wrong and does not
        // match the subtitle frame numbers:
        playPos = currentFrame->frameNumber;
    }
    else
    {
        // Use timecodes for time based SRT subtitles. Feeding this into
        // NormalizeVideoTimecode() should adjust for non-zero start times
        // and wraps. For MPEG, wraps will occur just once every 26.5 hours
        // and other formats less frequently so this should be sufficient.
        // Note: timecodes should now always be valid even in the case
        // when a frame doesn't have a valid timestamp. If an exception is
        // found where this is not true then we need to use the frameNumber
        // when timecode is not defined by uncommenting the following lines.
        //if (currentFrame->timecode == 0)
        //    playPos = (uint64_t)
        //        ((currentFrame->frameNumber / video_frame_rate) * 1000);
        //else
        playPos = m_player->GetDecoder()->NormalizeVideoTimecode(currentFrame->timecode);
    }
    if (playPos != 0)
        changed |= subs->HasSubtitleChanged(playPos);
    if (!changed)
    {
        subs->Unlock();
        return;
    }

    DeleteAllChildren();
    SetRedraw();
    QStringList rawsubs = subs->GetSubtitles(playPos);
    if (rawsubs.empty())
    {
        subs->Unlock();
        return;
    }

    // wrap text that is wider than the safe area
    QFontMetrics font(*(gTextSubFont->GetFace()));
    int maxwidth = m_safeArea.width();
    QStringList wrappedsubs;
    QString wrappedtext = "";
    int i = 0;
    while ((i < rawsubs.size()) || !wrappedtext.isEmpty())
    {
        QString nextline = wrappedtext;
        if (i < rawsubs.size())
            nextline += rawsubs[i].remove((const QRegExp&) m_removeHTML);
        wrappedtext = "";

        while (font.width(nextline) > maxwidth)
        {
            QString word = nextline.section(" ", -1, -1,
                                            QString::SectionSkipEmpty);
            if (word.isEmpty() || font.width(word) > maxwidth)
                break;
            wrappedtext = word + " " + wrappedtext;
            nextline.chop(word.size() + 1);
        }
        if (!nextline.isEmpty())
            wrappedsubs.append(nextline);
        i++;
    }
    subs->Unlock();

    int height = font.height();
    int y = m_safeArea.height() - (height * wrappedsubs.size());
    int centre = m_safeArea.width() / 2;
    QBrush bgfill = QBrush(QColor(0, 0, 0), Qt::SolidPattern);
    foreach (QString subtitle, wrappedsubs)
    {
        if (subtitle.isEmpty())
            continue;
        int width = font.width(subtitle);
        int x = centre - (width / 2);
        QRect rect(x, y, width, height);

        if (m_useBackground)
        {
            MythUIShape *shape = new MythUIShape(this,
                QString("tsubbg%1%2").arg(x).arg(y));
            shape->SetFillBrush(bgfill);
            shape->SetArea(MythRect(rect));
        }
        MythUIText* text = new MythUIText(subtitle, *gTextSubFont, rect,
                                rect, this,QString("tsub%1%2").arg(x).arg(y));
        if (text)
            text->SetJustification(Qt::AlignCenter);
        y += height;
        VERBOSE(VB_PLAYBACK, LOC + subtitle);
        m_refreshArea = true;
    }
}

void SubtitleScreen::DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos)
{
    if (!dvdButton || !m_player)
        return;

    VideoOutput *vo = m_player->getVideoOutput();
    if (!vo)
        return;

    DeleteAllChildren();
    SetRedraw();

    float tmp = 0.0;
    QRect dummy;
    vo->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);

    AVSubtitleRect *hl_button = dvdButton->rects[0];
    uint h = hl_button->h;
    uint w = hl_button->w;
    QRect rect = QRect(hl_button->x, hl_button->y, w, h);
    QImage bg_image(hl_button->pict.data[0], w, h, QImage::Format_Indexed8);
    uint32_t *bgpalette = (uint32_t *)(hl_button->pict.data[1]);

    bool blank = true;
    for (uint x = 0; x < w; x++)
    {
        for (uint y = 0; y < h; y++)
        {
            if (qAlpha(bgpalette[bg_image.pixelIndex(x, y)]) > 0)
            {
                blank = false;
                break;
            }
        }
    }

    if (!blank)
    {
        QVector<unsigned int> bg_palette;
        for (int i = 0; i < AVPALETTE_COUNT; i++)
            bg_palette.push_back(bgpalette[i]);
        bg_image.setColorTable(bg_palette);
        bg_image = bg_image.convertToFormat(QImage::Format_ARGB32);
        AddScaledImage(bg_image, rect);
        VERBOSE(VB_PLAYBACK, LOC + "Added DVD button background");
    }

    QImage fg_image = bg_image.copy(buttonPos);
    QVector<unsigned int> fg_palette;
    uint32_t *fgpalette = (uint32_t *)(dvdButton->rects[1]->pict.data[1]);
    for (int i = 0; i < AVPALETTE_COUNT; i++)
        fg_palette.push_back(fgpalette[i]);
    fg_image.setColorTable(fg_palette);

    // scale highlight image to match OSD size, if required
    QRect button = buttonPos.adjusted(0, 2, 0, 0);
    fg_image = fg_image.convertToFormat(QImage::Format_ARGB32);
    AddScaledImage(fg_image, button);
}

void SubtitleScreen::DisplayCC608Subtitles(void)
{
    static const QColor clr[8] =
    {
        Qt::white,   Qt::red,     Qt::green, Qt::yellow,
        Qt::blue,    Qt::magenta, Qt::cyan,  Qt::white,
    };

    if (!InitialiseFont() || !m_608reader)
        return;

    bool changed = false;

    if (m_player && m_player->getVideoOutput())
    {
        QRect oldsafe = m_safeArea;
        m_safeArea = m_player->getVideoOutput()->GetSafeRect();
        if (oldsafe != m_safeArea)
            changed = true;
    }
    else
    {
        return;
    }

    CC608Buffer* textlist = m_608reader->GetOutputText(changed);
    if (!changed)
        return;
    if (textlist)
        textlist->lock.lock();
    DeleteAllChildren();
    if (!textlist)
        return;
    if (textlist && textlist->buffers.empty())
    {
        textlist->lock.unlock();
        return;
    }

    QFontMetrics font(*(gTextSubFont->GetFace()));

    vector<CC608Text*>::iterator i = textlist->buffers.begin();
    bool teletextmode = (*i)->teletextmode;
    int xscale = teletextmode ? 40 : 36;
    int yscale = teletextmode ? 25 : 17;
    gTextSubFont->GetFace()->setPixelSize(m_safeArea.height() / (yscale * 1.2));
    QBrush bgfill = QBrush(QColor(0, 0, 0), Qt::SolidPattern);

    for (; i != textlist->buffers.end(); i++)
    {
        CC608Text *cc = (*i);

        if (cc && (cc->text != QString::null))
        {
            int width  = font.width(cc->text);
            int height = font.height();
            int x = teletextmode ? cc->y : (cc->x + 3);
            int y = teletextmode ? cc->x : cc->y;
            x = (int)(((float)x / (float)xscale) * (float)m_safeArea.width());
            y = (int)(((float)y / (float)yscale) * (float)m_safeArea.height());
            QRect rect(x, y, width, height);

            if (!teletextmode && m_useBackground)
            {
                MythUIShape *shape = new MythUIShape(this,
                    QString("cc608bg%1%2%3").arg(cc->x).arg(cc->y).arg(width));
                shape->SetFillBrush(bgfill);
                shape->SetArea(MythRect(rect));
            }

            gTextSubFont->SetColor(clr[max(min(0, cc->color), 7)]);
            MythUIText *text = new MythUIText(
                   cc->text, *gTextSubFont, rect, rect, (MythUIType*)this,
                   QString("cc608txt%1%2%3").arg(cc->x).arg(cc->y).arg(width));
            if (text)
                text->SetJustification(Qt::AlignCenter);
            m_refreshArea = true;
            VERBOSE(VB_VBI, QString("x %1 y %2 String: '%3'")
                                .arg(cc->x).arg(cc->y).arg(cc->text));
        }
    }
    textlist->lock.unlock();
}

void SubtitleScreen::DisplayCC708Subtitles(void)
{
    if (!m_708reader)
        return;

    CC708Service *cc708service = m_708reader->GetCurrentService();
    float video_aspect = 1.77777f;
    if (m_player && m_player->getVideoOutput())
    {
        video_aspect = m_player->GetVideoAspect();
        QRect oldsafe = m_safeArea;
        m_safeArea = m_player->getVideoOutput()->GetSafeRect();
        if (oldsafe != m_safeArea)
        {
            for (uint i = 0; i < 8; i++)
                cc708service->windows[i].changed = true;
            int size = (m_safeArea.height() * m_708fontZoom) / 2000;
            m_708fontSizes[1] = size;
            m_708fontSizes[0] = size * 32 / 42;
            m_708fontSizes[2] = size * 42 / 32;
        }
    }
    else
    {
        return;
    }

    if (!Initialise708Fonts())
        return;

    for (uint i = 0; i < 8; i++)
    {
        CC708Window &win = cc708service->windows[i];
        if (win.exists && win.visible && !win.changed)
            continue;

        Clear708Cache(i);
        if (!win.exists || !win.visible)
            continue;

        QMutexLocker locker(&win.lock);
        vector<CC708String*> list = win.GetStrings();
        if (list.size())
            Display708Strings(win, i, video_aspect, list);
        for (uint j = 0; j < list.size(); j++)
            delete list[j];
        win.changed = false;
    }
}

void SubtitleScreen::Clear708Cache(int num)
{
    if (!m_708imageCache[num].isEmpty())
    {
        foreach(MythUIType* image, m_708imageCache[num])
            DeleteChild(image);
        m_708imageCache[num].clear();
    }
}

void SubtitleScreen::Display708Strings(const CC708Window &win, int num,
                                       float aspect, vector<CC708String*> &list)
{
    VERBOSE(VB_VBI, LOC + QString("Display Win %1, Anchor_id %2, x_anch %3, "
                                  "y_anch %4, relative %5")
                .arg(num).arg(win.anchor_point).arg(win.anchor_horizontal)
                .arg(win.anchor_vertical).arg(win.relative_pos));

    bool display = false;
    MythFontProperties *mythfont;
    uint max_row_width = 0;
    uint total_height = 0;
    uint i = 0;
    for (uint row = 0; (row < win.true_row_count) && (i < list.size()); row++)
    {
        uint row_width = 0, max_row_height = 0;
        for (; (i < list.size()) && list[i] && (list[i]->y <= row); i++)
        {
            if (list[i]->y < row)
                continue;

            mythfont = Get708Font(list[i]->attr);
            if (!mythfont)
                continue;

            QString text = list[i]->str.trimmed();
            if (!text.isEmpty())
                display = true;

            QFontMetrics font(*(mythfont->GetFace()));
            uint height = (uint)font.height();

            row_width += font.width(list[i]->str);
            max_row_height = max(max_row_height, height);
        }

        max_row_width = max(max_row_width, row_width);
        total_height += max_row_height;
    }

    if (!display)
        return;

    float xrange  = win.relative_pos ? 100.0f :
                    (aspect > 1.4f) ? 210.0f : 160.0f;
    float yrange  = win.relative_pos ? 100.0f : 75.0f;
    float xmult   = (float)m_safeArea.width() / xrange;
    float ymult   = (float)m_safeArea.height() / yrange;
    uint anchor_x = (uint)(xmult * (float)win.anchor_horizontal);
    uint anchor_y = (uint)(ymult * (float)win.anchor_vertical);

    if (win.anchor_point % 3 == 1)
        anchor_x -= (((int)max_row_width) / 2);
    if (win.anchor_point % 3 == 2)
        anchor_x -= (int)max_row_width;
    if (win.anchor_point / 3 == 1)
        anchor_y -= (((int)total_height) / 2);
    if (win.anchor_point / 3 == 2)
        anchor_y -= (int)total_height;

    if (win.GetFillAlpha()) // TODO border?
    {
        QRect bg(anchor_x, anchor_y, max_row_width, total_height);
        QBrush fill(win.GetFillColor(), Qt::SolidPattern);
        MythUIShape *shape = new MythUIShape(this,
                QString("cc708bg%1").arg(num));
        shape->SetFillBrush(fill);
        shape->SetArea(MythRect(bg));
        m_708imageCache[num].append(shape);
        m_refreshArea = true;
    }

    i = 0;
    int y = anchor_y;
    for (uint row = 0; (row < win.true_row_count) && (i < list.size()); row++)
    {
        uint maxheight = 0;
        int  x = anchor_x;
        bool first = true;
        for (; (i < list.size()) && list[i] && (list[i]->y <= row); i++)
        {
            bool last = ((i + 1) == list.size());
            if (!last)
                last = (list[i + 1]->y > row);

            QString rawstring = list[i]->str;
            mythfont = Get708Font(list[i]->attr);

            if ((list[i]->y < row) || !mythfont || rawstring.isEmpty())
                continue;

            QString trimmed = rawstring.trimmed();
            if (!trimmed.size() && last)
                continue;

            QFontMetrics font(*(mythfont->GetFace()));
            uint height = (uint)font.height();
            maxheight   = max(maxheight, height);
            uint spacewidth = font.width(QString(" "));
            uint textwidth  = font.width(trimmed);

            int leading  = 0;
            int trailing = 0;
            if (trimmed.size() != rawstring.size())
            {
                if (trimmed.size())
                {
                    leading  = rawstring.indexOf(trimmed.at(0));
                    trailing = rawstring.size() - trimmed.size() - leading;
                }
                else
                {
                    leading = rawstring.size();
                }
                leading  *= spacewidth;
                trailing *= spacewidth;
            }

            bool background = list[i]->attr.GetBGAlpha();
            QBrush bgfill = QBrush((list[i]->attr.GetBGColor(), Qt::SolidPattern));

            if (leading && background && !first)
            {
                // draw background for leading space
                QRect space(x, y, leading, height);
                MythUIShape *shape = new MythUIShape(this,
                        QString("cc708shape%1x%2lead").arg(row).arg(i));
                shape->SetFillBrush(bgfill);
                shape->SetArea(MythRect(space));
                m_708imageCache[num].append(shape);
                m_refreshArea = true;
            }

            x += leading;
            QRect rect(x, y, textwidth, height);

            if (trimmed.size() && textwidth && background)
            {
                MythUIShape *shape = new MythUIShape(this,
                        QString("cc708shape%1x%2main").arg(row).arg(i));
                shape->SetFillBrush(bgfill);
                shape->SetArea(MythRect(rect));
                m_708imageCache[num].append(shape);
                m_refreshArea = true;
            }

            if (trimmed.size() && textwidth)
            {
                MythUIText *text = new MythUIText(list[i]->str, *mythfont,
                                                  rect, rect,
                                                  (MythUIType*)this,
                                                  QString("cc708text%1x%2").arg(row).arg(i));
                m_708imageCache[num].append(text);
                if (text)
                    text->SetJustification(Qt::AlignCenter);
                m_refreshArea = true;
            }

            x += textwidth;

            if (trailing && background && !last)
            {
                // draw background for trailing space
                QRect space(x, y, trailing, height);
                MythUIShape *shape = new MythUIShape(this,
                        QString("cc708shape%1x%2trail").arg(row).arg(i));
                shape->SetFillBrush(bgfill);
                shape->SetArea(MythRect(space));
                m_708imageCache[num].append(shape);
                m_refreshArea = true;
            }

            x += trailing;
            first = false;
            VERBOSE(VB_VBI, QString("Win %1 row %2 String '%3'")
                .arg(num).arg(row).arg(list[i]->str));
        }
        y += maxheight;
    }
}

void SubtitleScreen::AddScaledImage(QImage &img, QRect &pos)
{
    VideoOutput *vo = m_player->getVideoOutput();
    if (!vo)
        return;

    QRect scaled = vo->GetImageRect(pos);
    if (scaled.size() != pos.size())
    {
        img = img.scaled(scaled.width(), scaled.height(),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    MythPainter *osd_painter = vo->GetOSDPainter();
    MythImage* image = NULL;
    if (osd_painter)
         image = osd_painter->GetFormatImage();

    if (image)
    {
        image->Assign(img);
        MythUIImage *uiimage = new MythUIImage(this, "dvd_button");
        if (uiimage)
        {
            m_refreshArea = true;
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
        }
    }
}

bool SubtitleScreen::InitialiseFont(void)
{
    static bool initialised = false;
    if (initialised)
        return gTextSubFont;

    QString font = "FreeMono";

    MythFontProperties *mythfont = new MythFontProperties();
    if (mythfont)
    {
        QFont newfont(font);
        font.detach();
        mythfont->SetFace(newfont);
        gTextSubFont = mythfont;
    }
    else
        return false;

    initialised = true;
    VERBOSE(VB_PLAYBACK, LOC + QString("Loaded main subtitle font"));
    return true;
}

bool SubtitleScreen::Initialise708Fonts(void)
{
    static bool initialised = false;
    if (initialised)
        return true;

    initialised = true;

    VERBOSE(VB_IMPORTANT, "Initialise708Fonts()");

    // TODO remove extra fonts from settings page
    QStringList fonts;
    fonts.append("Droid Sans Mono"); // default
    fonts.append("FreeMono");        // mono serif
    fonts.append("DejaVu Serif");    // prop serif
    fonts.append("Droid Sans Mono"); // mono sans
    fonts.append("Liberation Sans"); // prop sans
    fonts.append("Purisa");          // casual
    fonts.append("URW Chancery L");  // cursive
    fonts.append("Impact");          // capitals

    int count = 0;
    foreach(QString font, fonts)
    {
        MythFontProperties *mythfont = new MythFontProperties();
        if (mythfont)
        {
            QFont newfont(font);
            font.detach();
            mythfont->SetFace(newfont);
            gCC708Fonts.insert(count, mythfont);
            count++;
        }
    }
    VERBOSE(VB_PLAYBACK, LOC + QString("Loaded %1 CEA-708 fonts").arg(count));
    return true;
}

MythFontProperties* SubtitleScreen::Get708Font(CC708CharacterAttribute attr)
{
    MythFontProperties *mythfont = gCC708Fonts[attr.font_tag & 0x7];
    if (!mythfont)
        return NULL;

    mythfont->GetFace()->setItalic(attr.italics);
    mythfont->GetFace()->setPixelSize(m_708fontSizes[attr.pen_size & 0x3]);
    mythfont->GetFace()->setUnderline(attr.underline);
    mythfont->SetColor(attr.GetFGColor());

    int off = m_708fontSizes[attr.pen_size & 0x3] / 20;
    QPoint shadowsz(off, off);
    QColor colour = attr.GetEdgeColor();
    int alpha     = attr.GetFGAlpha();
    bool outline = false;
    bool shadow  = false;

    if (attr.edge_type == k708AttrEdgeLeftDropShadow)
    {
        shadow = true;
        shadowsz.setX(-off);
    }
    else if (attr.edge_type == k708AttrEdgeRightDropShadow)
    {
        shadow = true;
    }
    else if (attr.edge_type == k708AttrEdgeUniform ||
             attr.edge_type == k708AttrEdgeRaised  ||
             attr.edge_type == k708AttrEdgeDepressed)
    {
        outline = true;
    }

    mythfont->SetOutline(outline, colour, off, alpha);
    mythfont->SetShadow(shadow, shadowsz, colour, alpha);

    return mythfont;
}
