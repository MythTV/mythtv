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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 *
 * This library contains code originally obtained from the libstroke
 * library, which was written by Mark F. Willey.  If I am in offense
 * of any copyright or anything, please let me know and I will make
 * the appropriate fixes.
 */

#include "mythgesture.h"

#include <cmath>
#include <algorithm>

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
    QMutex m;
    QMap <QString, MythGestureEvent::Gesture> sequences;
};



/* comments in header */
MythGesture::MythGesture(size_t max_points, size_t min_points,
                         size_t max_sequence, size_t scale_ratio,
                         float bin_percent) :
    m_recording(false), min_x(10000), max_x(-1), min_y(10000), max_y(-1),
    max_points(max_points), min_points(min_points), max_sequence(max_sequence),
    scale_ratio(scale_ratio), bin_percent(bin_percent)
{
    /* default to an invalid event */
    last_gesture = MythGestureEvent::MaxGesture;

    /* create new private information */
    p = new MythGesturePrivate();

    /* Click */
    p->sequences.insert("5", MythGestureEvent::Click);

    /* Lines */
    p->sequences.insert("456", MythGestureEvent::Right);
    p->sequences.insert("654", MythGestureEvent::Left);
    p->sequences.insert("258", MythGestureEvent::Down);
    p->sequences.insert("852", MythGestureEvent::Up);

    /* Diagonals */
    p->sequences.insert("951", MythGestureEvent::UpLeft);
    p->sequences.insert("753", MythGestureEvent::UpRight);
    p->sequences.insert("159", MythGestureEvent::DownRight);
    p->sequences.insert("357", MythGestureEvent::DownLeft);

    /* Double Lines*/
    p->sequences.insert("96321",MythGestureEvent::UpThenLeft);
    p->sequences.insert("74123",MythGestureEvent::UpThenRight);
    p->sequences.insert("36987",MythGestureEvent::DownThenLeft);
    p->sequences.insert("14789",MythGestureEvent::DownThenRight);
    p->sequences.insert("32147",MythGestureEvent::LeftThenDown);
    p->sequences.insert("98741",MythGestureEvent::LeftThenUp);
    p->sequences.insert("12369",MythGestureEvent::RightThenDown);
    p->sequences.insert("78963",MythGestureEvent::RightThenUp);
}

MythGesture::~MythGesture()
{
    delete p;
}

/* comments in header */
void MythGesture::adjustExtremes(int x, int y)
{
    min_x = std::min(min_x, x);
    max_x = std::max(max_y, x);
    min_y = std::min(min_y, y);
    max_y = std::max(max_y, y);
}

bool MythGesture::recording(void) const
{
    p->m.lock();
    bool temp = m_recording;
    p->m.unlock();
    return temp;
}

/* comments in header */
void MythGesture::start(void)
{
    p->m.lock();
    m_recording = true;
    p->m.unlock();
}

/* comments in header */
void MythGesture::stop(void)
{
    p->m.lock();

    if (m_recording)
    {
        m_recording = false;

        /* translate before resetting maximums */
        last_gesture = p->sequences[translate()];

        min_x = min_y = 10000;
        max_x = max_y = -1;
    }

    p->m.unlock();
}

MythGestureEvent *MythGesture::gesture(void) const
{
    return new MythGestureEvent(last_gesture);
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
    size_t total_points = points.count();

    if (total_points > max_points)
    {
        points.clear();
        return "0";
    }

    /* treat any stroke with less than the minimum number of points as
     * a click (not a drag), which is the center bin */
    if (total_points < min_points)
    {
        points.clear();
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

    /* bin boundary and size variables */
    int delta_x, delta_y;
    int bound_x_1, bound_x_2;
    int bound_y_1, bound_y_2;

    /* determine size of grid */
    delta_x = max_x - min_x;
    delta_y = max_y - min_y;

    /* calculate bin boundary positions */
    bound_x_1 = min_x + (delta_x / 3);
    bound_x_2 = min_x + 2 * (delta_x / 3);

    bound_y_1 = min_y + (delta_y / 3);
    bound_y_2 = min_y + 2 * (delta_y / 3);

    if (delta_x > scale_ratio * delta_y)
    {
        bound_y_1 = (max_y + min_y - delta_x) / 2 + (delta_x / 3);
        bound_y_2 = (max_y + min_y - delta_x) / 2 + 2 * (delta_x / 3);
    }
    else if (delta_y > scale_ratio * delta_x)
    {
        bound_x_1 = (max_x + min_x - delta_y) / 2 + (delta_y / 3);
        bound_x_2 = (max_x + min_x - delta_y) / 2 + 2 * (delta_y / 3);
    }

    /* build string by placing points in bins, collapsing bins and
       discarding those with too few points... */

    while (!points.empty())
    {
        QPoint p = points.front();
        points.pop_front();

        /* figure out which bin the point falls in */
        current_bin = determineBin(p, bound_x_1, bound_x_2, bound_y_1,
                                   bound_y_2);

        /* if this is the first point, consider it the previous bin, too. */
        prev_bin = (prev_bin == 0) ? current_bin : prev_bin;

        if (prev_bin == current_bin) 
            bin_count++;
        else 
        {

            /* we are moving to a new bin -- consider adding to the
               sequence */
            if ((bin_count > (total_points * bin_percent)) || first_bin)
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
    if (sequence_count > max_sequence) 
        sequence = '0';

    return sequence;
}

/* comments in header */
bool MythGesture::record(const QPoint & p)
{
    /* only record if we haven't exceeded the maximum points */
    if (((uint)points.size() >= max_points) || !recording())
        return false;

    if (points.size() == 0)
    {
        points.push_back(p);
        return true;
    }

    /* interpolate between last and current point */
    int delx = p.x() - points.back().x();
    int dely = p.y() - points.back().y();

    /* step by the greatest delta direction */
    if (abs(delx) > abs(dely))
    {
        float iy = points.back().y();

        /* go from the last point to the current, whatever direction
         * it may be */
        for (float ix = points.back().x();
             (delx > 0) ? (ix < p.x()) : (ix > p.x());
             ix += (delx > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            iy += std::fabs(((float) dely / (float) delx))
                * (float) ((dely < 0) ? -1.0 : 1.0);

            points.push_back(QPoint((int)ix, (int)iy));

            adjustExtremes((int)ix, (int)iy);
        }
    }
    else /* same thing, but for dely larger than delx case... */
    {
        float ix = points.back().x();

        /* go from the last point to the current, whatever direction
           it may be */
        for (float iy = points.back().y();
             (dely > 0) ? (iy < p.y()) : (iy > p.y());
             iy += (dely > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            ix += std::fabs(((float) delx / (float) dely))
                * (float) ((delx < 0) ? -1.0 : 1.0);

            /* add the interpolated point */
            points.push_back(QPoint((int)ix, (int)iy));

            adjustExtremes((int)ix, (int)iy);
        }
    }

    points.push_back(p);

    return true;
}


static const char *gesturename[] = {
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
    "Click",
    "MaxGesture"
};

/* comments in header */
MythGestureEvent::operator QString() const
{
    return gesturename[m_gesture];
}
