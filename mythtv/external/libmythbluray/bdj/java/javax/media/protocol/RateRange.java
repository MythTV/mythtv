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

package javax.media.protocol;

public class RateRange {
    public RateRange(RateRange r)
    {
        this.current = r.current;
        this.min = r.min;
        this.max = r.max;
        this.isExact = r.isExact;
    }

    public RateRange(float init, float min, float max, boolean isExact)
    {
        this.current = init;
        this.min = min;
        this.max = max;
        this.isExact = isExact;
    }

    public float setCurrentRate(float rate)
    {
        this.current = rate;
        return current;
    }

    public float getCurrentRate()
    {
        return current;
    }

    public float getMinimumRate()
    {
        return min;
    }

    public float getMaximumRate()
    {
        return max;
    }

    public boolean isExact()
    {
        return isExact;
    }

    private float current;
    private float min;
    private float max;
    private boolean isExact;
}
