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

import java.net.MalformedURLException;
import java.net.URL;

public class MediaLocator
{
    public MediaLocator(URL url) { 
        this(url.toExternalForm());
    }

    public MediaLocator(String locatorString) { 
        int index = locatorString.indexOf(":");
        if (index <= 0)
            throw new IllegalArgumentException("Bad locator string.");
        protocol = locatorString.substring(0, index);
        remainder = locatorString.substring(index + 1);
    }

    public URL getURL() throws MalformedURLException {
        return new URL(toExternalForm());
    }

    public String getProtocol() {
        return protocol;
    }

    public String getRemainder() {
        return remainder;
    }

    public String toString() {
        return toExternalForm();
    }

    public String toExternalForm() {
        return protocol + ":" + remainder;
    }
    
    private String protocol = "";
    private String remainder = "";
}
