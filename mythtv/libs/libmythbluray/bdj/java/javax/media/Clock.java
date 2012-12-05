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

package javax.media;

public interface Clock {
    public void setTimeBase(TimeBase master)
            throws IncompatibleTimeBaseException;

    public void syncStart(Time at);

    public void stop();

    public void setStopTime(Time stopTime);

    public Time getStopTime();

    public void setMediaTime(Time now);

    public Time getMediaTime();

    public long getMediaNanoseconds();

    public Time getSyncTime();

    public TimeBase getTimeBase();

    public Time mapToTimeBase(Time t) throws ClockStoppedException;

    public float getRate();

    public float setRate(float factor);

    public static final Time RESET = null;
}
