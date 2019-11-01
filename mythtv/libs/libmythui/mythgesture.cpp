/* -*- myth -*- */
/**
 * @file mythgesture.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief A C++ ripoff of the stroke library, modified for MythTV.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * This library contains code originally obtained from the libstroke
 * library, which was written by Mark F. Willey.  If I am in offense
 * of any copyright or anything, please let me know and I will make
 * the appropriate fixes.
 */

#include "mythgesture.h"

#include <cmath>
#include <algorithm>
#include <complex>

#include <QMutex>
#include <QMap>

QEvent::Type MythGestureEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

/**
 * @class MythGesturePrivate
 * @brief Private information used only by a stroke class.
 */
class MythGesturePrivate {

public:
    QMutex m_m;
    QMap <QString, MythGestureEvent::Gesture> m_sequences;
};



/* comments in header */
MythGesture::MythGesture(size_t max_points, size_t min_points,
                         size_t max_sequence, size_t scale_ratio,
                         float bin_percent) :
    m_max_points(max_points), m_min_points(min_points), m_max_sequence(max_sequence),
    m_scale_ratio(scale_ratio), m_bin_percent(bin_percent)
{
    /* default to an invalid event */
    m_last_gesture = MythGestureEvent::MaxGesture;

    /* create new private information */
    p = new MythGesturePrivate();

    /* Click */
    p->m_sequences.insert("5", MythGestureEvent::Click);

    /* Lines */
    p->m_sequences.insert("456", MythGestureEvent::Right);
    p->m_sequences.insert("654", MythGestureEvent::Left);
    p->m_sequences.insert("258", MythGestureEvent::Down);
    p->m_sequences.insert("852", MythGestureEvent::Up);

    /* Diagonals */
    p->m_sequences.insert("951", MythGestureEvent::UpLeft);
    p->m_sequences.insert("753", MythGestureEvent::UpRight);
    p->m_sequences.insert("159", MythGestureEvent::DownRight);
    p->m_sequences.insert("357", MythGestureEvent::DownLeft);

    /* Double Lines*/
    p->m_sequences.insert("96321",MythGestureEvent::UpThenLeft);
    p->m_sequences.insert("74123",MythGestureEvent::UpThenRight);
    p->m_sequences.insert("36987",MythGestureEvent::DownThenLeft);
    p->m_sequences.insert("14789",MythGestureEvent::DownThenRight);
    p->m_sequences.insert("32147",MythGestureEvent::LeftThenDown);
    p->m_sequences.insert("98741",MythGestureEvent::LeftThenUp);
    p->m_sequences.insert("12369",MythGestureEvent::RightThenDown);
    p->m_sequences.insert("78963",MythGestureEvent::RightThenUp);
    p->m_sequences.insert("45654",MythGestureEvent::RightThenLeft);
    p->m_sequences.insert("65456",MythGestureEvent::LeftThenRight);
    p->m_sequences.insert("85258",MythGestureEvent::UpThenDown);
    p->m_sequences.insert("25852",MythGestureEvent::DownThenUp);
}

MythGesture::~MythGesture()
{
    delete p;
}

/* comments in header */
void MythGesture::adjustExtremes(int x, int y)
{
    m_min_x = std::min(m_min_x, x);
    m_max_x = std::max(m_max_x, x);
    m_min_y = std::min(m_min_y, y);
    m_max_y = std::max(m_max_y, y);
}

bool MythGesture::recording(void) const
{
    p->m_m.lock();
    bool temp = m_recording;
    p->m_m.unlock();
    return temp;
}

/* comments in header */
void MythGesture::start(void)
{
    p->m_m.lock();
    m_recording = true;
    p->m_m.unlock();
}

/* comments in header */
void MythGesture::stop(void)
{
    p->m_m.lock();

    if (m_recording)
    {
        m_recording = false;

        /* translate before resetting maximums */
        m_last_gesture = p->m_sequences[translate()];

        m_min_x = m_min_y = 10000;
        m_max_x = m_max_y = -1;
    }

    p->m_m.unlock();
}

MythGestureEvent *MythGesture::gesture(void) const
{
    return new MythGestureEvent(m_last_gesture);
}

/* comments in header */
static int determineBin (const QPoint & p, int x1, int x2, int y1, int y2)
{
    int bin_num = 1;
    if (p.x() > x1)
        bin_num += 1;
    if (p.x() > x2)
        bin_num += 1;
    if (p.y() > y1)
        bin_num += 3;
    if (p.y() > y2)
        bin_num += 3;

    return bin_num;
}

/* comments in header */
QString MythGesture::translate(void)
{
    size_t total_points = m_points.count();

    if (total_points > m_max_points)
    {
        m_points.clear();
        return "0";
    }

    /* treat any stroke with less than the minimum number of points as
     * a click (not a drag), which is the center bin */
    if (total_points < m_min_points)
    {
        m_points.clear();
        return "5";
    }

    QString sequence;

    /* number of bins recorded in the stroke */
    size_t sequence_count = 0;

    /* points-->sequence translation scratch variables */
    int prev_bin = 0;
    int current_bin = 0;
    int bin_count = 0;

    /*flag indicating the start of a stroke - always count it in the sequence*/
    bool first_bin = true;

    /* determine size of grid */
    int delta_x = m_max_x - m_min_x;
    int delta_y = m_max_y - m_min_y;

    /* calculate bin boundary positions */
    int bound_x_1 = m_min_x + (delta_x / 3);
    int bound_x_2 = m_min_x + 2 * (delta_x / 3);

    int bound_y_1 = m_min_y + (delta_y / 3);
    int bound_y_2 = m_min_y + 2 * (delta_y / 3);

    if (delta_x > m_scale_ratio * delta_y)
    {
        bound_y_1 = (m_max_y + m_min_y - delta_x) / 2 + (delta_x / 3);
        bound_y_2 = (m_max_y + m_min_y - delta_x) / 2 + 2 * (delta_x / 3);
    }
    else if (delta_y > m_scale_ratio * delta_x)
    {
        bound_x_1 = (m_max_x + m_min_x - delta_y) / 2 + (delta_y / 3);
        bound_x_2 = (m_max_x + m_min_x - delta_y) / 2 + 2 * (delta_y / 3);
    }

    /* build string by placing points in bins, collapsing bins and
       discarding those with too few points... */

    while (!m_points.empty())
    {
        QPoint pt = m_points.front();
        m_points.pop_front();

        /* figure out which bin the point falls in */
        current_bin = determineBin(pt, bound_x_1, bound_x_2, bound_y_1,
                                   bound_y_2);

        /* if this is the first point, consider it the previous bin, too. */
        prev_bin = (prev_bin == 0) ? current_bin : prev_bin;

        if (prev_bin == current_bin)
            bin_count++;
        else
        {

            /* we are moving to a new bin -- consider adding to the
               sequence */
            if ((bin_count > (total_points * m_bin_percent)) || first_bin)
            {
                first_bin = false;
                sequence += '0' + prev_bin;
                sequence_count ++;
            }

            /* restart counting points in the new bin */
            bin_count = 0;
            prev_bin = current_bin;
        }
    }

    /* add the last run of points to the sequence */
    sequence += '0' + current_bin;
    sequence_count++;

    /* bail out on error cases */
    if (sequence_count > m_max_sequence)
        sequence = '0';

    return sequence;
}

/* comments in header */
bool MythGesture::record(const QPoint & pt)
{
    /* only record if we haven't exceeded the maximum points */
    if (((uint)m_points.size() >= m_max_points) || !recording())
        return false;

    if (m_points.empty())
    {
        m_points.push_back(pt);
        return true;
    }

    /* interpolate between last and current point */
    int delx = pt.x() - m_points.back().x();
    int dely = pt.y() - m_points.back().y();

    /* step by the greatest delta direction */
    if (abs(delx) > abs(dely))
    {
        float iy = m_points.back().y();

        /* go from the last point to the current, whatever direction
         * it may be */
        for (float ix = m_points.back().x();
             (delx > 0) ? (ix < pt.x()) : (ix > pt.x());
             ix += (delx > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            iy += std::fabs(((float) dely / (float) delx))
                * (float) ((dely < 0) ? -1.0 : 1.0);

            m_points.push_back(QPoint((int)ix, (int)iy));

            adjustExtremes((int)ix, (int)iy);
        }
    }
    else /* same thing, but for dely larger than delx case... */
    {
        float ix = m_points.back().x();

        /* go from the last point to the current, whatever direction
           it may be */
        for (float iy = m_points.back().y();
             (dely > 0) ? (iy < pt.y()) : (iy > pt.y());
             iy += (dely > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            ix += std::fabs(((float) delx / (float) dely))
                * (float) ((delx < 0) ? -1.0 : 1.0);

            /* add the interpolated point */
            m_points.push_back(QPoint((int)ix, (int)iy));

            adjustExtremes((int)ix, (int)iy);
        }
    }

    m_points.push_back(pt);

    return true;
}


static const char *gesturename[] = {
    "Unknown",
    "Up",
    "Down",
    "Left",
    "Right",
    "UpLeft",
    "UpRight",
    "DownLeft",
    "DownRight",
    "UpThenLeft",
    "UpThenRight",
    "DownThenLeft",
    "DownThenRight",
    "LeftThenUp",
    "LeftThenDown",
    "RightThenUp",
    "RightThenDown",
    "RightThenLeft",
    "LeftThenRight",
    "UpThenDown",
    "DownThenUp",
    "Click",
    "MaxGesture"
};

/* comments in header */
MythGestureEvent::operator QString() const
{
    return gesturename[m_gesture];
}
