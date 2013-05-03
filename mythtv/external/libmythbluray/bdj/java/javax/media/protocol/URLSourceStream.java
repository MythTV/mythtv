/*
 * This file is part of libbluray
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
import java.io.InputStream;
import java.net.URLConnection;

class URLSourceStream
    implements PullSourceStream
{
    protected URLConnection conn;
    protected InputStream stream;
    protected boolean eosReached;
    ContentDescriptor contentType;

    public URLSourceStream(URLConnection conn, ContentDescriptor type)
        throws IOException
    {
        this.conn = conn;
        this.stream = conn.getInputStream();
        this.eosReached = false;
        this.contentType = type;
    }

    public ContentDescriptor getContentDescriptor()
    {
        return this.contentType;
    }

    public boolean willReadBlock()
    {
        if (this.eosReached == true)
            return true;
        try
        {
            return this.stream.available() == 0;
        }
        catch (IOException e)
        {
        }
        return true;
    }

    public int read(byte[] buffer, int offset, int length)
        throws IOException
    {
        int bytesRead = this.stream.read(buffer, offset, length);
        if (bytesRead == -1)
            this.eosReached = true;
        return bytesRead;
    }

    public void close()
        throws IOException
    {
        this.stream.close();
    }

    public boolean endOfStream()
    {
        return this.eosReached;
    }

    public Object[] getControls()
    {
        return new Object[0];
    }

    public Object getControl(String controlName)
    {
        return null;
    }

    public long getContentLength()
    {
        long len = this.conn.getContentLength();
        len = len == -1L ? -1L : len;

        return len;
    }
}
