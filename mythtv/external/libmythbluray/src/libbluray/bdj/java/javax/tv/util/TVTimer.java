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

import org.videolan.Logger;

public abstract class TVTimer
{
    public static TVTimer getTimer() {
        Logger.unimplemented(TVTimer.class.getName(), "getTimer");
        return null;
    }

    public abstract TVTimerSpec scheduleTimerSpec(TVTimerSpec paramTVTimerSpec)
        throws TVTimerScheduleFailedException;

    public abstract long getGranularity();

    public abstract long getMinRepeatInterval();

    public abstract void deschedule(TVTimerSpec paramTVTimerSpec);
}
