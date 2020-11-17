/*
 *  Class MythTimer
 *
 *  Copyright (C) Rune Petersen 2012
 *  Copyright (C) Daniel Kristjansson 2013
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

#include <cstdint>

// MythTV includes
#include "mythtimer.h"

//#define DEBUG_TIMER_API_USAGE
#ifdef DEBUG_TIMER_API_USAGE
#undef NDEBUG
#include <cassert>
#endif

/** Creates a timer.
 *
 *  If a start state of kStartRunning is passed in the timer is
 *  started immediately as if start() had been called.
 */
MythTimer::MythTimer(StartState state)
{
    if (kStartRunning == state)
        start();
    else
        stop();
}

/// starts measuring elapsed time.
void MythTimer::start(void)
{
    m_timer.start();
    m_offset = 0ms;
}

/** Returns milliseconds elapsed since last start() or restart()
 *  and resets the count.
 *
 * \note If timer had not been started before this is called,
 *       this returns zero.
 *
 * \note If addMSecs() has been called and the total offset applied
 *       is negative then this can return a negative number.
 */
std::chrono::milliseconds MythTimer::restart(void)
{
    if (m_timer.isValid())
    {
        auto val = std::chrono::milliseconds(m_timer.restart()) + m_offset;
        m_offset = 0ms;
        return val;
    }
    start();
    return 0ms;
}

/** Stops timer, next call to isRunning() will return false and
 *  any calls to elapsed() or restart() will return 0 until after
 *  timer is restarted by a call to start() or restart().
 */
void MythTimer::stop(void)
{
    m_timer.invalidate();
}

/** Returns milliseconds elapsed since last start() or restart()
 *
 * \note If timer had not been started before this is called,
 *       this returns zero.
 *
 * \note If addMSecs() has been called and the total offset applied
 *       is negative then this can return a negative number.
 */
std::chrono::milliseconds MythTimer::elapsed(void)
{
    if (!m_timer.isValid())
    {
#ifdef DEBUG_TIMER_API_USAGE
        assert(0 == "elapsed called without timer being started");
#endif
        return 0ms;
    }

    auto e = std::chrono::milliseconds(m_timer.elapsed());
    if (!QElapsedTimer::isMonotonic() && (e > (24h - 100s)))
    {
        start();
        e = 0ms;
    }

    return e + m_offset;
}

/** Returns nanoseconds elapsed since last start() or restart()
 *
 * \note If timer had not been started before this is called,
 *       this returns zero.
 *
 * \note If addMSecs() has been called and the total offset applied
 *       is negative then this can return a negative number.
 */
std::chrono::nanoseconds MythTimer::nsecsElapsed(void) const
{
    if (!m_timer.isValid())
    {
#ifdef DEBUG_TIMER_API_USAGE
        assert(0 == "elapsed called without timer being started");
#endif
        return 0ns;
    }

    return std::chrono::nanoseconds(m_timer.nsecsElapsed()) + m_offset;
}

/** Returns true if start() or restart() has been called at least
 *  once since construction and since any call to stop().
 */
bool MythTimer::isRunning(void) const
{
    return m_timer.isValid();
}

/** Adds an offset to the last call to start() or restart().
 *
 *  This offset is removed if start() or restart() is called
 *  again. The offset may be positive or negative. This
 *  may be called multiple times to accrete various offsets.
 */
void MythTimer::addMSecs(std::chrono::milliseconds ms)
{
    m_offset += ms;
}
