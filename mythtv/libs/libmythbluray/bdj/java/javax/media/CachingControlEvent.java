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

public class CachingControlEvent extends ControllerEvent {
    public CachingControlEvent(Controller from, CachingControl cacheControl,
            long progress)
    {
        super(from);

        this.cacheControl = cacheControl;
        this.progress = progress;
    }

    public CachingControl getCachingControl()
    {
        return cacheControl;
    }

    public long getContentProgress()
    {
        return progress;
    }

    private CachingControl cacheControl;
    private long progress;
    private static final long serialVersionUID = 8877038236738665179L;
}
