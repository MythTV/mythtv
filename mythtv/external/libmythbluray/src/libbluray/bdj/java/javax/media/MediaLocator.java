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
import java.net.MalformedURLException;
import java.net.URL;

public class MediaLocator implements Serializable
{
    public MediaLocator(URL url) {
        this.url = url;
        if (url != null) {
            this.locatorString = url.toString().trim();
        }
    }

    public MediaLocator(String locatorString) {
        if (locatorString != null)
            this.locatorString = locatorString.trim();
    }

    public URL getURL() throws MalformedURLException {
        if (url == null) {
            url = new URL(locatorString);
        }
        return url;
    }

    public String getProtocol() {
        String protocol = "";
        int col = locatorString.indexOf(":");
        if (col >= 0) {
            protocol = locatorString.substring(0, col);
        }
        return protocol;
    }

    public String getRemainder() {
        String remainder = "";
        int col = locatorString.indexOf(":");
        if (col >= 0) {
            remainder = locatorString.substring(col + 1);
        }
        return remainder;
    }

    public String toString() {
        return toExternalForm();
    }

    public String toExternalForm() {
        return locatorString;
    }

    private String locatorString = "";
    private URL url = null;

    private static final long serialVersionUID = 1L;
}
