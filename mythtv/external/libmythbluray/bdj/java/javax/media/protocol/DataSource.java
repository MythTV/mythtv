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

import java.io.IOException;

import javax.media.Duration;
import javax.media.MediaLocator;

public abstract class DataSource implements Controls, Duration {
    public DataSource()
    {
    }

    public DataSource(MediaLocator source)
    {
        locator = source;
    }

    public void setLocator(MediaLocator source)
    {
        locator = source;
    }

    public MediaLocator getLocator()
    {
        return locator;
    }

    public abstract String getContentType();

    public abstract void connect() throws IOException;

    public abstract void disconnect();

    public abstract void start() throws IOException;

    public abstract void stop() throws IOException;

    protected void initCheck()
    {
        if (locator == null)
            throw new Error("Locator can't be null");
    }

    private MediaLocator locator = null;
}
