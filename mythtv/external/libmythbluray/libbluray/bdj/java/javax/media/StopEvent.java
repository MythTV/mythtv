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

import javax.media.Controller;

public class StopEvent extends TransitionEvent {
    public StopEvent(Controller source, int previous, int current, int target,
            Time mediaTime)
    {
        super(source, previous, current, target);

        this.mediaTime = mediaTime;
    }

    public Time getMediaTime()
    {
        return mediaTime;
    }

    private Time mediaTime;
    private static final long serialVersionUID = -4224879148377028872L;
}
