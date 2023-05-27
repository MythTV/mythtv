// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include <algorithm>

#include <QCoreApplication>

#include "output.h"
#include "visual.h"

class QObject;

const QEvent::Type OutputEvent::kPlaying =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kBuffering =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kInfo =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kPaused =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kStopped =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kError =
    (QEvent::Type) QEvent::registerEventType();

void OutputListeners::error(const QString &e)
{
    OutputEvent event(e);
    dispatch(event);
}

void OutputListeners::addVisual(MythTV::Visual *v)
{
    auto it = std::find(m_visuals.begin(), m_visuals.end(), v);
    if (it == m_visuals.end())
        m_visuals.push_back(v);
}

void OutputListeners::removeVisual(MythTV::Visual *v)
{
    auto it = std::find(m_visuals.begin(), m_visuals.end(), v);
    if (it != m_visuals.end())
        m_visuals.erase(it);
}

void OutputListeners::dispatchVisual(uchar *buffer, unsigned long b_len,
                                     std::chrono::milliseconds timecode,
                                     int chan, int prec)
{
    if (! buffer)
       return;

    for (auto & visual : m_visuals)
    {
        QMutexLocker locker(visual->mutex());
        visual->add(buffer, b_len, timecode, chan, prec);
    }
}

void OutputListeners::prepareVisuals()
{
    for (auto & visual : m_visuals)
    {
        QMutexLocker locker(visual->mutex());
        visual->prepare();
    }
}
