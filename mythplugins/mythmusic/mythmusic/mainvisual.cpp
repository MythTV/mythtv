// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

// C
#include <cmath>
#include <cstdio>

// C++
#include <algorithm>
#include <iostream>
using namespace std;

// Qt
#include <QPainter>

// mythtv
#include <mythuivideo.h>

// mythmusic
#include "visualize.h"
#include "mainvisual.h"
#include "constants.h"
#include "musicplayer.h"

// fast inlines
#include "inlines.h"


///////////////////////////////////////////////////////////////////////////////
// MainVisual

QStringList MainVisual::visualizers = MainVisual::Visualizations(false);
int MainVisual::currentVisualizer = gCoreContext->GetNumSetting("MusicLastVisualizer", 0);

MainVisual::MainVisual(MythUIVideo *visualiser)
    : QObject(NULL), MythTV::Visual(), m_visualiserVideo(visualiser),
      m_vis(NULL), m_playing(false), m_fps(20), m_samples(SAMPLES_DEFAULT_SIZE),
      m_updateTimer(NULL)
{
    setObjectName("MainVisual");

    resize(m_visualiserVideo->GetArea().size());

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000 / m_fps);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(timeout()));
}

MainVisual::~MainVisual()
{
    if (m_vis)
        delete m_vis;

    delete m_updateTimer;

    while (!m_nodes.empty())
        delete m_nodes.takeLast();

    gCoreContext->SaveSetting("MusicLastVisualizer", currentVisualizer);
}

void MainVisual::stop(void)
{
    m_updateTimer->stop();

    if (m_vis)
    {
        delete m_vis;
        m_vis = NULL;
    }
}

void MainVisual::setVisual(const QString &name)
{
    int index = visualizers.indexOf(name);

    if (index == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MainVisual: visualizer %1 not found!").arg(name));
        return;
    }

    currentVisualizer = index;

    m_pixmap.fill(m_visualiserVideo->GetBackgroundColor());

    QString visName, pluginName;

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

    m_updateTimer->stop();

    if (m_vis)
    {
        delete m_vis;
        m_vis = NULL;
    }

    for (const VisFactory* pVisFactory = VisFactory::VisFactories();
         pVisFactory; pVisFactory = pVisFactory->next())
    {
        if (pVisFactory->name() == visName)
        {
            m_vis = pVisFactory->create(this, pluginName);
            m_vis->resize(m_visualiserVideo->GetArea().size());
            m_fps = m_vis->getDesiredFPS();
            m_samples = m_vis->getDesiredSamples();
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
void MainVisual::add(uchar *buffer, unsigned long b_len, unsigned long timecode, int source_channels, int bits_per_sample)
{
    unsigned long len = b_len, cnt;
    short *l = 0, *r = 0;

    // len is length of buffer in fully converted samples
    len /= source_channels;
    len /= (bits_per_sample / 8);

    if (len > m_samples)
        len = m_samples;

    cnt = len;

    if (source_channels == 2)
    {
        l = new short[len];
        r = new short[len];

        if (bits_per_sample == 8)
            stereo16_from_stereopcm8(l, r, buffer, cnt);
        else if (bits_per_sample == 16)
            stereo16_from_stereopcm16(l, r, (short *) buffer, cnt);
    }
    else if (source_channels == 1)
    {
        l = new short[len];

        if (bits_per_sample == 8)
            mono16_from_monopcm8(l, buffer, cnt);
        else if (bits_per_sample == 16)
            mono16_from_monopcm16(l, (short *) buffer, cnt);
    }
    else
        len = 0;

    m_nodes.append(new VisualNode(l, r, len, timecode));
}

void MainVisual::timeout()
{
    VisualNode *node = NULL;
    if (m_playing && gPlayer->getOutput())
    {
        QMutexLocker locker(mutex());
        int64_t timestamp = gPlayer->getOutput()->GetAudiotime();
        while (m_nodes.size() > 1)
        {
            if ((int64_t)m_nodes.first()->offset > timestamp)
                break;

            if (m_vis)
                m_vis->processUndisplayed(node);

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
        if (m_vis->draw(&p, m_visualiserVideo->GetBackgroundColor()))
            m_visualiserVideo->UpdateFrame(&m_pixmap);
    }

    if (m_playing && !stop)
        m_updateTimer->start();
}

void MainVisual::resize(const QSize &size)
{
    m_pixmap = QPixmap(size);
    m_pixmap.fill(m_visualiserVideo->GetBackgroundColor());

    if (m_vis)
        m_vis->resize(size);
}

void MainVisual::customEvent(QEvent *event)
{
    if ((event->type() == OutputEvent::Playing)   ||
        (event->type() == OutputEvent::Info)      ||
        (event->type() == OutputEvent::Buffering) ||
        (event->type() == OutputEvent::Paused))
    {
        m_playing = true;
        if (!m_updateTimer->isActive())
            m_updateTimer->start();
    }
    else if ((event->type() == OutputEvent::Stopped) ||
             (event->type() == OutputEvent::Error))
    {
        m_playing = false;
    }
}

// static member function
QStringList MainVisual::Visualizations(bool showall)
{
    // TODO remove this once the visualiser selector is reinstated
    showall = true;

    QStringList visualizations;

    if (showall)
    {
        for (const VisFactory* pVisFactory = VisFactory::VisFactories();
            pVisFactory; pVisFactory = pVisFactory->next())
        {
            pVisFactory->plugins(&visualizations);
        }

        visualizations.sort();

        return visualizations;
    }

    visualizations = gCoreContext->GetSetting("VisualMode").split(';', QString::SkipEmptyParts);

    if (!visualizations.count())
        visualizations.push_front("Blank");

    return visualizations;
}
