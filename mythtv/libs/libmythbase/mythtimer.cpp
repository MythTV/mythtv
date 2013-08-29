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
MythTimer::MythTimer(StartState state) : m_offset(0)
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
    m_offset = 0;
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
int MythTimer::restart(void)
{
    if (m_timer.isValid())
    {
        int val = static_cast<int>(m_timer.restart() + m_offset);
        m_offset = 0;
        return val;
    }
    return start(), 0;
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
int MythTimer::elapsed(void) const
{
    if (!m_timer.isValid())
    {
#ifdef DEBUG_TIMER_API_USAGE
        assert(0 == "elapsed called without timer being started");
#endif
        return 0;
    }

    int64_t e = m_timer.elapsed();
    if (!m_timer.isMonotonic() && (e > 86300000))
    {
        const_cast<MythTimer*>(this)->start();
        e = 0;
    }

    return static_cast<int>(e + m_offset);
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
void MythTimer::addMSecs(int ms)
{
    m_offset += ms;
}
