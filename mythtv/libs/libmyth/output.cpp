// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include <algorithm>
using namespace std;

#include <QCoreApplication>

#include "output.h"
#include "visual.h"

class QObject;

QEvent::Type OutputEvent::Playing =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OutputEvent::Buffering =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OutputEvent::Info =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OutputEvent::Paused =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OutputEvent::Stopped =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OutputEvent::Error =
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
                        unsigned long written, int chan, int prec)
{
    if (! buffer)
       return;

    auto it = m_visuals.begin();
    for (; it != m_visuals.end(); ++it)
    {
        QMutexLocker locker((*it)->mutex());
        (*it)->add(buffer, b_len, written, chan, prec);
    }
}

void OutputListeners::prepareVisuals()
{
    auto it = m_visuals.begin();
    for (; it != m_visuals.end(); ++it)
    {
        QMutexLocker locker((*it)->mutex());
        (*it)->prepare();
    }
}
