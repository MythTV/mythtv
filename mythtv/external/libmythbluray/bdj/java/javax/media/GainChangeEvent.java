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

import java.util.EventObject;

public class GainChangeEvent extends EventObject implements MediaEvent {
    public GainChangeEvent(GainControl source, boolean mute, float dB,
            float level)
    {
        super(source);
        this.source = source;
        this.mute = mute;
        this.dB = dB;
        this.level = level;
    }

    public Object getSource()
    {
        return source;
    }

    public GainControl getSourceGainControl()
    {
        return source;
    }

    public boolean getMute()
    {
        return mute;
    }

    public float getDB()
    {
        return dB;
    }

    public float getLevel()
    {
        return level;
    }

    private GainControl source;
    private boolean mute;
    private float dB;
    private float level;
    private static final long serialVersionUID = -3685931181525562650L;
}
