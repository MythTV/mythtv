/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package org.bluray.ui;

import javax.media.Time;

public class FrameAccurateAnimationTimer {
    public static FrameAccurateAnimationTimer getInstance(Time start, Time stop)
            throws IllegalArgumentException {
        if (start == null || stop == null ||
            start.getNanoseconds() >= stop.getNanoseconds()) {
            throw new IllegalArgumentException("FrameAccurateAnimationTimer.getInstance()");
        }

        return new FrameAccurateAnimationTimer(start, stop);
    }

    private FrameAccurateAnimationTimer(Time start, Time stop)
            throws IllegalArgumentException {
        this.start = start;
        this.stop  = stop;
    }

    FrameAccurateAnimationTimer(FrameAccurateAnimationTimer t) {
        start = new Time(t.start.getNanoseconds());
        stop = new Time(t.stop.getNanoseconds());
    }

    public Time getStartTime() {
        return start;
    }

    public Time getStopTime() {
        return stop;
    }

    public String toString() {
        return "FrameAccurateAnimationTimer [StartTime=" + start.getNanoseconds() +
                ", StopTime=" + stop.getNanoseconds() + "]";
    }

    private Time start;
    private Time stop;
}
