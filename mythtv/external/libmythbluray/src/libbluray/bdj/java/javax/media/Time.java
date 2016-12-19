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

import java.io.Serializable;

public class Time implements Serializable {
    public static final long ONE_SECOND = 1000000000L;
    protected long nanoseconds;

    public Time(long nano) {
        this.nanoseconds = nano;
    }

    public Time(double seconds) {
        this.nanoseconds = secondsToNanoseconds(seconds);
    }

    protected long secondsToNanoseconds(double seconds) {
        return (long) (seconds * ONE_SECOND);
    }

    public long getNanoseconds() {
        return nanoseconds;
    }

    public double getSeconds() {
        return nanoseconds / (double) ONE_SECOND;
    }

    private static final long serialVersionUID = 1L;
}
