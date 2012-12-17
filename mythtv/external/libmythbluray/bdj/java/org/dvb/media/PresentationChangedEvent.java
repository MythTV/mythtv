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

package org.dvb.media;

import javax.media.ControllerEvent;
import javax.media.Controller;
import javax.media.MediaLocator;

public class PresentationChangedEvent extends ControllerEvent {
    public PresentationChangedEvent(Controller source, MediaLocator stream,
            int reason)
    {
        super(source);

        this.stream = stream;
        this.reason = reason;
    }

    public MediaLocator getStream()
    {
        return stream;
    }

    public int getReason()
    {
        return reason;
    }

    public static final int STREAM_UNAVAILABLE = 0x00;
    public static final int CA_FAILURE = 0x01;
    public static final int CA_RETURNED = 0x02;

    private MediaLocator stream;
    private int reason;
    private static final long serialVersionUID = 7465215844040053752L;
}
