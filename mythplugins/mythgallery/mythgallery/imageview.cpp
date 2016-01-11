// -*- Mode: c++ -*-
/* ============================================================
 * File  : imageview.cpp
 * Description :
 *
 * Copyright 2004-2006 Renchi Raju, Daniel Kristjansson
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// STL headers
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Qt headers
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QRunnable>
#include <QWaitCondition>

// MythTV plugin headers
#include <mythcontext.h>
#include <lcddevice.h>
#include <mthread.h>
#include <mythuihelper.h>

// MythGallery headers
#include "imageview.h"
#include "galleryutil.h"
#include "thumbgenerator.h"

// Tuning parameter for seasonal weights, between 0 and 1, where lower numbers
// give greater weight to seasonal photos. The leading beta shape controls
// dates that are approaching and the trailing beta shape controls dates that
// just passed. When these are set to 0.175 and 0.31, respectively, about one
// quarter of the photos are from the upcoming week in prior years and about one
// quarter of the photos are from the preceding month in prior years.
const double LEADING_BETA_SHAPE = 0.175;
const double TRAILING_BETA_SHAPE = 0.31;
// Photos without a timestamp will default to the mode of the beta distribution.
const double DEFAULT_WEIGHT = std::pow(0.5, TRAILING_BETA_SHAPE - 1) *
            std::pow(0.5, LEADING_BETA_SHAPE - 1);
// The edges of the distribution get clipped to avoid a singularity.
const qint64 BETA_CLIP = 60 * 60 * 24;

class ImageView::LoadAlbumRunnable : public QRunnable {
    ImageView *m_parent;
    ThumbList m_dirList;
    int m_sortorder;
    int m_slideshow_sequencing;
    QMutex m_isAliveLock;
    bool m_isAlive;

public:
    LoadAlbumRunnable(ImageView *parent, const ThumbList &roots, int sortorder,
            int slideshow_sequencing);

    void abort();
    virtual void run();

    /** Separate the input into files and directories */
    static void filterDirectories(const ThumbList &input,
            ThumbList &fileList, ThumbList &dirList);
};

ImageView::ImageView(const ThumbList &itemList,
                     int *pos, int slideShow, int sortorder)
    : m_screenSize(640,480),
      m_wmult(1.0f),
      m_hmult(1.0f),
      m_pos(*pos),
      m_savedPos(pos),
      m_movieState(0),
      m_zoom(1.0f),

      // Info variables
      m_info_show(false),
      m_info_show_short(false),

      // Common slideshow variables
      m_slideshow_running(false),
      m_slideshow_sequencing(slideShow),
      m_slideshow_frame_delay(2),
      m_slideshow_frame_delay_state(m_slideshow_frame_delay * 1000),
      m_slideshow_timer(NULL),

      // Common effect state variables
      m_effect_running(false),
      m_effect_current_frame(0),
      m_effect_method(QString::null),
      m_effect_random(false),

      m_loaderRunnable(NULL),
      m_listener(this),
      m_loaderThread(NULL),
      m_slideshow_sequence(ComposeSlideshowSequence(slideShow)),
      m_finishedLoading(false)
{

    int xbase, ybase, screenwidth, screenheight;
    GetMythUI()->GetScreenSettings(xbase, screenwidth,  m_wmult,
                                ybase, screenheight, m_hmult);
    m_screenSize = QSize(screenwidth, screenheight);

    // --------------------------------------------------------------------

    bool recurse = gCoreContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    ThumbItem *origItem = NULL;
    if (m_pos < itemList.size())
        origItem = itemList.at(m_pos);

    ThumbList fileList, dirList;
    LoadAlbumRunnable::filterDirectories(itemList, fileList, dirList);
    AddItems(fileList);

    if (recurse)
    {
        // Load pictures from all directories on a different thread.
        m_loaderRunnable = new LoadAlbumRunnable(this, dirList, sortorder,
                                                 m_slideshow_sequencing);
        m_loaderThread = new MThread("LoadAlbum", m_loaderRunnable);
        QObject::connect(m_loaderThread->qthread(), SIGNAL(finished()),
                &m_listener, SLOT(FinishLoading()));
        m_loaderThread->start();

        // Wait for at least one image to be loaded.
        {
            QMutexLocker guard(&m_itemListLock);
            while (m_itemList.empty() && !m_finishedLoading)
            {
                m_imagesLoaded.wait(&m_itemListLock);
            }
        }
    }

    // --------------------------------------------------------------------

    // since we remove dirs item position might have changed
    if (origItem)
        m_pos = m_itemList.indexOf(origItem);

    m_pos = (!origItem || (m_pos == -1)) ? 0 : m_pos;
    m_slideshow_sequence->set(m_pos);

    // --------------------------------------------------------------------

    m_slideshow_frame_delay = gCoreContext->GetNumSetting("SlideshowDelay", 0);
    m_slideshow_frame_delay = (!m_slideshow_frame_delay) ?
        2 : m_slideshow_frame_delay;
    m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;

    // --------------------------------------------------------------------

    if (slideShow == 2)
    {
        m_slideshow_mode = QT_TR_NOOP("Random Slideshow");
    }
    else if (slideShow == 3)
    {
        m_slideshow_mode = QT_TR_NOOP("Seasonal Slideshow");
    }
    else
    {
        m_slideshow_mode = QT_TR_NOOP("Slideshow");
    }
}

ImageView::~ImageView()
{
    UpdateLCD(NULL);
    if (m_loaderRunnable && m_loaderThread)
    {
        m_loaderRunnable->abort();
        m_loaderThread->wait();
    }

    if (m_slideshow_sequence)
    {
        delete m_slideshow_sequence;
        m_slideshow_sequence = NULL;
    }

    if (m_loaderRunnable)
    {
        delete m_loaderRunnable;
        m_loaderRunnable = NULL;
    }

    if (m_loaderThread)
    {
        delete m_loaderThread;
        m_loaderThread = NULL;
    }

    *m_savedPos = m_pos;
}

SequenceBase *ImageView::ComposeSlideshowSequence(int slideshow_sequencing) {
    switch (slideshow_sequencing)
    {
        case 2:
            return new SequenceShuffle();
        case 3:
            return new SequenceWeighted();
        default:
            return new SequenceInc();
    }
}

QString ImageView::GetRandomEffect(void) const
{
    QMap<QString,QString> tmpMap = m_effect_map;
    tmpMap.remove("none");
    tmpMap.remove("Ken Burns (gl)");
    QStringList t = tmpMap.keys();
    int i = (int) ( (float)(t.count()) * random() / (RAND_MAX + 1.0f) );
    return tmpMap[t[i]];
}

void ImageView::UpdateLCD(const ThumbItem *item)
{
    LCD *lcd = LCD::Get();
    if (!lcd)
        return;

    if (!item)
    {
        lcd->setFunctionLEDs(FUNC_PHOTO, false);
        lcd->switchToTime();
        return;
    }
    lcd->setFunctionLEDs(FUNC_PHOTO, true);

    QString name = item->GetName();
    QString desc = QString::number(m_pos + 1) + " / " +
        QString::number(m_itemList.size());

    QList<LCDTextItem> textItems;
    textItems.append(LCDTextItem(
                         1, ALIGN_CENTERED, name, "Generic", true));
    textItems.append(LCDTextItem(
                         2, ALIGN_CENTERED, desc, "Generic", false));

    lcd->switchToGeneric(textItems);
}

QString ImageView::GetDescriptionStatus(void) const
{
    if (m_slideshow_running)
        return " [" + tr(m_slideshow_mode) + "]";

    return "";
}

void ImageView::GetScreenShot(QImage& image, const ThumbItem *item)
{
    QFileInfo fi(item->GetPath());
    QString screenshot = QString("%1%2-screenshot.jpg")
                                .arg(ThumbGenerator::getThumbcacheDir(fi.path()))
                                .arg(item->GetName());

    if (QFile::exists(screenshot))
    {
        QImage img(screenshot);
        image = img;
    }
    else
    {
        QString movie("gallery-moviethumb.png");
        if (GetMythUI()->FindThemeFile(movie))
            image.load(movie);
    }
}

void ImageView::AddItems(const ThumbList &itemList)
{
    QMutexLocker guard(&m_itemListLock);

    m_itemList.append(itemList);

    m_slideshow_sequence->extend(itemList.size());

    if (m_slideshow_sequencing == 3)
    {
        for (int i = 0; i < itemList.size(); ++i)
        {
            ThumbItem *item = itemList.at(i);
            double weight = GetSeasonalWeight(item);
            static_cast<SequenceWeighted *>(m_slideshow_sequence)->add(weight);
        }
    }

    if (!m_itemList.empty())
    {
        m_imagesLoaded.wakeAll();
    }
}

ThumbItem *ImageView::getCurrentItem() const
{
    QMutexLocker guard(&m_itemListLock);
    return m_itemList.at(m_pos);
}

ThumbItem *ImageView::advanceItem()
{
    QMutexLocker guard(&m_itemListLock);
    m_pos = m_slideshow_sequence->next();
    return m_itemList.at(m_pos);
}

ThumbItem *ImageView::retreatItem()
{
    QMutexLocker guard(&m_itemListLock);
    m_pos = m_slideshow_sequence->prev();
    return m_itemList.at(m_pos);
}

void ImageView::FinishLoading()
{
    QMutexLocker guard(&m_itemListLock);
    m_finishedLoading = true;
    m_imagesLoaded.wakeAll();
}

/**
 * This method calculates a weight for the item based on how closely it was
 * taken to the current time of year. This means that New Year's pictures will
 * be displayed very frequently on every New Year's, and that anniversary
 * pictures will be favored again every anniversary. The weights are chosen
 * using a beta distribution with a tunable shape parameter.
 */
double ImageView::GetSeasonalWeight(ThumbItem *item) {
    item->InitTimestamp();
    if (item->HasTimestamp())
    {
        QDateTime timestamp = item->GetTimestamp();
        QDateTime now = QDateTime::currentDateTime();
        QDateTime curYearAnniversary = QDateTime(QDate(
            now.date().year(),
            timestamp.date().month(),
            timestamp.date().day()),
            timestamp.time());
        bool isAnniversaryPast = curYearAnniversary < now;
        QDateTime adjacentYearAnniversary = QDateTime(QDate(
            now.date().year() + (isAnniversaryPast ? 1 : -1),
            timestamp.date().month(),
            timestamp.date().day()),
            timestamp.time());
        double range = std::abs(
            curYearAnniversary.secsTo(adjacentYearAnniversary)) + BETA_CLIP;
        // This calculation is not normalized, because that would require the
        // beta function, which isn't part of the C++98 libraries. Weights
        // that aren't normalized work just as well relative to each other.
        double weight = std::pow(abs(now.secsTo(
                isAnniversaryPast ? curYearAnniversary : adjacentYearAnniversary
            ) + BETA_CLIP) / range,
            TRAILING_BETA_SHAPE - 1) *
            std::pow(abs(now.secsTo(
                isAnniversaryPast ? adjacentYearAnniversary : curYearAnniversary
            ) + BETA_CLIP) / range,
            LEADING_BETA_SHAPE - 1);
        return weight;
    }
    else
    {
        return DEFAULT_WEIGHT;
    }
}

ImageView::LoadAlbumRunnable::LoadAlbumRunnable(
        ImageView *parent, const ThumbList &roots, int sortorder,
        int slideshow_sequencing)
    : m_parent(parent),
      m_dirList(roots),
      m_sortorder(sortorder),
      m_slideshow_sequencing(slideshow_sequencing),
      m_isAlive(true)
{
}

/** Request that all processing stop */
void ImageView::LoadAlbumRunnable::abort()
{
    QMutexLocker guard(&m_isAliveLock);
    m_isAlive = false;
}

void ImageView::LoadAlbumRunnable::run()
{
    while (!m_dirList.empty())
    {
        ThumbItem *dir = m_dirList.takeFirst();
        ThumbList children;
        GalleryUtil::LoadDirectory(children, dir->GetPath(),
                                   GalleryFilter(m_sortorder != 0),
                                   false, NULL, NULL);

        {
            QMutexLocker guard(&m_isAliveLock);
            if (!m_isAlive)
            {
                break;
            }
        }

        // The first images should not always come from the first directory.
        if (m_slideshow_sequencing > 1)
        {
            std::random_shuffle(children.begin(), children.end());
        }

        ThumbList fileList;
        filterDirectories(children, fileList, m_dirList);
        if (!fileList.empty())
        {
            m_parent->AddItems(fileList);
        }
    }
}

/** Separate the input into files and directories */
void ImageView::LoadAlbumRunnable::filterDirectories(const ThumbList &input,
        ThumbList &fileList, ThumbList &dirList)
{
    for (int i = 0; i < input.size(); ++i)
    {
        ThumbItem *item = input.at(i);
        ThumbList &targetList = item->IsDir() ? dirList : fileList;
        targetList.append(item);
    }
}

LoadAlbumListener::LoadAlbumListener(ImageView *parent)
    : m_parent(parent)
{
}

LoadAlbumListener::~LoadAlbumListener()
{
}

void LoadAlbumListener::FinishLoading() const
{
    m_parent->FinishLoading();
}
