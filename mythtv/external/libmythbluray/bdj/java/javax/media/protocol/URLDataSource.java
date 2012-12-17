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
import java.net.URL;
import java.net.URLConnection;
import javax.media.Time;
import javax.media.Duration;
import javax.media.MediaLocator;

public class URLDataSource extends PullDataSource {
    protected URLDataSource()
    {
    }

    public URLDataSource(URL url) throws IOException
    {
        setLocator(new MediaLocator(url));
        this.connected = false;
    }

    public PullSourceStream[] getStreams()
    {
        if (!this.connected)
            throw new Error("Unconnected source.");
        return this.sources;
    }

    public void connect() throws IOException
    {
        this.conn = getLocator().getURL().openConnection();
        this.conn.connect();
        this.connected = true;
        String str = this.conn.getContentType();
        if (str == null)
            str = "UnknownContent";
        this.contentType = new ContentDescriptor(ContentDescriptor.mimeTypeToPackageName(str));
        this.sources = new URLSourceStream[1];
        this.sources[0] = new URLSourceStream(this.conn, this.contentType);
    }

    public String getContentType()
    {
        if (!this.connected)
            throw new Error("Source is unconnected.");
        return this.contentType.getContentType();
    }

    public void disconnect()
    {
        if (this.connected)
        {
            try
            {
                this.sources[0].close();
            }
            catch (IOException localIOException)
            {
            }
            this.connected = false;
        }
    }

    public void start() throws IOException
    {
    }

    public void stop() throws IOException
    {
    }

    public Time getDuration()
    {
        return Duration.DURATION_UNKNOWN;
    }

    public Object[] getControls()
    {
        return new Object[0];
    }

    public Object getControl(String controlName)
    {
        return null;
    }

    protected URLConnection conn;
    protected ContentDescriptor contentType;
    protected boolean connected;
    private URLSourceStream[] sources;
}
