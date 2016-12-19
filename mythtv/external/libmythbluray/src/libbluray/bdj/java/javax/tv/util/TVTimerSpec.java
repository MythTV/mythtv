/*
 * This file is part of libbluray
 * Copyright (C) 2016  VideoLAN
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

package javax.tv.util;

import java.util.Enumeration;
import java.util.Vector;

import org.videolan.Logger;

public class TVTimerSpec
{
    private boolean absolute;
    private boolean regular;
    private boolean repeat;
    private long    time;

    public TVTimerSpec() {
        absolute = true;
        repeat = false;
        regular = true;
        time = 0L;
    }

    public long getTime() {
        return time;
    }

    public boolean isAbsolute() {
        return absolute;
    }

    public boolean isRegular() {
        return regular;
    }

    public boolean isRepeat() {
        return repeat;
    }

    public void addTVTimerWentOffListener(TVTimerWentOffListener l) {
        Logger.unimplemented(TVTimer.class.getName(), "addTVTimerWentOffListener");
    }

    public void removeTVTimerWentOffListener(TVTimerWentOffListener l) {
        Logger.unimplemented(TVTimer.class.getName(), "removeTVTimerWentOffListener");
    }

    public void notifyListeners(TVTimer source) {
        Logger.unimplemented(TVTimer.class.getName(), "notifyListeners");
    }

    public void setAbsolute(boolean absolute) {
        this.absolute = absolute;
    }

    public void setAbsoluteTime(long when) {
        if (when < 0L) {
            throw new IllegalArgumentException();
        }
        setAbsolute(true);
        setTime(when);
        setRepeat(false);
    }

    public void setDelayTime(long delay) {
        if (delay < 0L) {
            throw new IllegalArgumentException();
        }
        setAbsolute(false);
        setTime(delay);
        setRepeat(false);
    }

    public void setRegular(boolean regular) {
        this.regular = regular;
    }

    public void setRepeat(boolean repeat) {
        this.repeat = repeat;
    }

    public void setTime(long time) {
        if (time < 0L)
            throw new IllegalArgumentException();
        this.time = time;
    }
}
