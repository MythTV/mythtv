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
    protected URLDataSource() {
    }

    public URLDataSource(URL url) throws IOException {
        setLocator(new MediaLocator(url));
        connected = false;
    }

    public PullSourceStream[] getStreams() {
        if (!connected)
            throw new Error("Unconnected source.");
        return sources;
    }

    public void connect() throws IOException {
        conn = getLocator().getURL().openConnection();
        conn.connect();
        connected = true;
        String str = conn.getContentType();
        if (str == null)
            str = "UnknownContent";
        contentType = new ContentDescriptor(ContentDescriptor.mimeTypeToPackageName(str));
        sources = new URLSourceStream[1];
        sources[0] = new URLSourceStream(conn, contentType);
    }

    public String getContentType() {
        if (!connected)
            throw new Error("Source is unconnected.");
        return contentType.getContentType();
    }

    public void disconnect() {
        if (connected) {
            try {
                sources[0].close();
            } catch (IOException ioe) {
            }
            connected = false;
        }
    }

    public void start() throws IOException {
    }

    public void stop() throws IOException {
    }

    public Time getDuration() {
        return Duration.DURATION_UNKNOWN;
    }

    public Object[] getControls() {
        return new Object[0];
    }

    public Object getControl(String controlName) {
        return null;
    }

    protected URLConnection conn;
    protected ContentDescriptor contentType;
    protected boolean connected;
    private URLSourceStream[] sources;
}
