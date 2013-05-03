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

package javax.tv.media;

import java.awt.Rectangle;

public class AWTVideoSize
{
    public AWTVideoSize(Rectangle source, Rectangle dest) { 
        this.source = source;
        this.dest = dest;
    }

    public Rectangle getSource() {
        return (Rectangle)source.clone();
    }

    public Rectangle getDestination() {
        return (Rectangle)dest.clone();
    }

    public float getXScale() {
        return getDestination().width / getSource().width;
    }

    public float getYScale() {
        return getDestination().height / getSource().height;
    }

    public int hashCode()
    {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((dest == null) ? 0 : dest.hashCode());
        result = prime * result + ((source == null) ? 0 : source.hashCode());
        return result;
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (obj == null)
            return false;
        if (getClass() != obj.getClass())
            return false;
        AWTVideoSize other = (AWTVideoSize) obj;
        if (dest == null) {
            if (other.dest != null)
                return false;
        } else if (!dest.equals(other.dest))
            return false;
        if (source == null) {
            if (other.source != null)
                return false;
        } else if (!source.equals(other.source))
            return false;
        return true;
    }

    public String toString()
    {
        return "AWTVideoSize [dest=" + dest + ", source=" + source + "]";
    }
    
    private Rectangle source;
    private Rectangle dest;
}
