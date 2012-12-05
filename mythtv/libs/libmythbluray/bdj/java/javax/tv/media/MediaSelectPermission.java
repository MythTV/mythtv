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

import java.security.Permission;
import javax.tv.locator.Locator;
import java.io.Serializable;

public final class MediaSelectPermission extends Permission
    implements Serializable
{
    public MediaSelectPermission(Locator locator) {
        super("javax.tv.media.MediaSelectPermission");

        if (locator == null)
            this.locator = "*";
        else
        this.locator = locator.toExternalForm();
    }

    public MediaSelectPermission(String locator, String actions) {
        super("javax.tv.media.MediaSelectPermission");

        this.locator = locator;
    }

    public boolean implies(Permission perm) {
        return (perm instanceof MediaSelectPermission) && (this.equals(perm) || this.locator.equals("*"));
    }

    public boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (getClass() != obj.getClass())
            return false;
        MediaSelectPermission other = (MediaSelectPermission) obj;
        if (locator == null) {
            if (other.locator != null)
                return false;
        } else if (!locator.equals(other.locator))
            return false;
        return true;
    }

    public int hashCode() {
        final int prime = 31;
        int result = prime + ((locator == null) ? 0 : locator.hashCode());
        return result;
    }

    public String getActions() {
        return "";
    }

    private String locator;
    private static final long serialVersionUID = 128534275081685853L;
}
