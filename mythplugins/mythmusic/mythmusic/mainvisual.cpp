// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

// C++
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

// Qt
#include <QPainter>

// MythTV
#include <libmythui/mythuivideo.h>

// mythmusic
#include "constants.h"
#include "mainvisual.h"
#include "musicplayer.h"
#include "visualize.h"

// fast inlines
#include "inlines.h"


///////////////////////////////////////////////////////////////////////////////
// MainVisual

MainVisual::MainVisual(MythUIVideo *visualizer)
    : QObject(nullptr), m_visualizerVideo(visualizer)
{
    setObjectName("MainVisual");

    for (const VisFactory* pVisFactory = VisFactory::VisFactories();
        pVisFactory; pVisFactory = pVisFactory->next())
    {
        pVisFactory->plugins(&m_visualizers);
    }
    m_visualizers.sort();

    m_currentVisualizer = gCoreContext->GetNumSetting("MusicLastVisualizer", 0);

    resize(m_visualizerVideo->GetArea().size());

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000 / m_fps);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &MainVisual::timeout);
}

MainVisual::~MainVisual()
{
    m_updateTimer->stop();
    delete m_updateTimer;

    delete m_vis;

    while (!m_nodes.empty())
        delete m_nodes.takeLast();

    gCoreContext->SaveSetting("MusicLastVisualizer", m_currentVisualizer);
}

void MainVisual::stop(void)
{
    m_updateTimer->stop();

    if (m_vis)
    {
        delete m_vis;
        m_vis = nullptr;
    }
}

void MainVisual::setVisual(const QString &name)
{
    m_updateTimer->stop();

    int index = m_visualizers.indexOf(name);

    if (index == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MainVisual: visualizer %1 not found!").arg(name));
        return;
    }

    m_currentVisualizer = index;

    m_pixmap.fill(m_visualizerVideo->GetBackgroundColor());

    QString visName;
    QString pluginName;

    if (name.contains("-"))
    {
        visName = name.section('-', 0, 0);
        pluginName = name.section('-', 1, 1);
    }
    else
    {
        visName = name;
        pluginName.clear();
    }

    if (m_vis)
    {
        delete m_vis;
        m_vis = nullptr;
    }

    for (const VisFactory* pVisFactory = VisFactory::VisFactories();
         pVisFactory; pVisFactory = pVisFactory->next())
    {
        if (pVisFactory->name() == visName)
        {
            m_vis = pVisFactory->create(this, pluginName);
            m_vis->resize(m_visualizerVideo->GetArea().size());
            m_fps = m_vis->getDesiredFPS();
            m_samples = m_vis->getDesiredSamples();

            QMutexLocker locker(mutex());
            prepare();

            break;
        }
    }

    // force an update
    m_updateTimer->start(1000 / m_fps);
}

// Caller holds mutex() lock
void MainVisual::prepare()
{
    while (!m_nodes.empty())
        delete m_nodes.takeLast();
}

// This is called via : mythtv/libs/libmyth/output.cpp :: OutputListeners::dispatchVisual
//    from : mythtv/libs/libmyth/audio/audiooutputbase.cpp :: AudioOutputBase::AddData
// Caller holds mutex() lock
void MainVisual::add(const void *buffer, unsigned long b_len,
                     std::chrono::milliseconds timecode,
                     int source_channels, int bits_per_sample)
{
    unsigned long len = b_len;
    short *l = nullptr;
    short *r = nullptr;
    bool s32le = false;

    // 24 bit samples are stored as s32le in the buffer.
    // 32 bit samples are stored as float. Flag the difference.
    if (bits_per_sample == 24)
    {
        s32le = true;
        bits_per_sample = 32;
    }

    // len is length of buffer in fully converted samples
    len /= source_channels;
    len /= (bits_per_sample / 8);

    len = std::min(len, m_samples);

    int cnt = len;

    if (source_channels == 2)
    {
        l = new short[len];
        r = new short[len];

        if (bits_per_sample == 8)
            stereo16_from_stereopcm8(l, r, (uchar *) buffer, cnt);
        else if (bits_per_sample == 16)
            stereo16_from_stereopcm16(l, r, (short *) buffer, cnt);
        else if (s32le)
            stereo16_from_stereopcm32(l, r, (int *) buffer, cnt);
        else if (bits_per_sample == 32)
            stereo16_from_stereopcmfloat(l, r, (float *) buffer, cnt);
        else
            len = 0;
    }
    else if (source_channels == 1)
    {
        l = new short[len];

        if (bits_per_sample == 8)
            mono16_from_monopcm8(l, (uchar *) buffer, cnt);
        else if (bits_per_sample == 16)
            mono16_from_monopcm16(l, (short *) buffer, cnt);
        else if (s32le)
            mono16_from_monopcm32(l, (int *) buffer, cnt);
        else if (bits_per_sample == 32)
            mono16_from_monopcmfloat(l, (float *) buffer, cnt);
        else
            len = 0;
    }
    else
    {
        len = 0;
    }

    m_nodes.append(new VisualNode(l, r, len, timecode));
}

void MainVisual::timeout()
{
    VisualNode *node = nullptr;
    if (m_playing && gPlayer->getOutput())
    {
        QMutexLocker locker(mutex());
        std::chrono::milliseconds timestamp = gPlayer->getOutput()->GetAudiotime();
        while (m_nodes.size() > 1)
        {
            // LOG(VB_PLAYBACK, LOG_DEBUG,
            //  QString("\tMV %1 first > %2 timestamp").
            //  arg(m_nodes.first()->m_offset.count()).arg(timestamp.count()));
            if (m_nodes.first()->m_offset >= timestamp + 5s)
            {
                // REW seek: drain buffer and start over
            }
            else if (m_nodes.first()->m_offset > timestamp)
            {
                break;          // at current time
            }

            if (m_vis)
                m_vis->processUndisplayed(m_nodes.first());

            delete m_nodes.first();
            m_nodes.removeFirst();
        }

        if (!m_nodes.isEmpty())
            node = m_nodes.first();
    }

    bool stop = true;
    if (m_vis)
        stop = m_vis->process(node);

    if (m_vis && !stop)
    {
        QPainter p(&m_pixmap);
        if (m_vis->draw(&p, m_visualizerVideo->GetBackgroundColor()))
            m_visualizerVideo->UpdateFrame(&m_pixmap);
    }

    if (m_playing && !stop)
        m_updateTimer->start();
}

void MainVisual::resize(const QSize size)
{
    m_pixmap = QPixmap(size);
    m_pixmap.fill(m_visualizerVideo->GetBackgroundColor());

    if (m_vis)
        m_vis->resize(size);
}

void MainVisual::customEvent(QEvent *event)
{
    if ((event->type() == OutputEvent::kPlaying)   ||
        (event->type() == OutputEvent::kInfo)      ||
        (event->type() == OutputEvent::kBuffering) ||
        (event->type() == OutputEvent::kPaused))
    {
        m_playing = true;
        if (!m_updateTimer->isActive())
            m_updateTimer->start();
    }
    else if ((event->type() == OutputEvent::kStopped) ||
             (event->type() == OutputEvent::kError))
    {
        m_playing = false;
    }
}
