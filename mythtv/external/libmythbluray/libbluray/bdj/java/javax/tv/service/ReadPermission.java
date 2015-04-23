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

package javax.tv.service;

import java.security.Permission;
import javax.tv.locator.Locator;
import java.io.Serializable;

public final class ReadPermission extends Permission implements Serializable {
    public ReadPermission(Locator locator)
    {
        super(locator.toExternalForm());
        
        this.locator = locator.toExternalForm();
    }

    public ReadPermission(String locator, String actions)
    {
        super(null);
        
        this.locator = locator;
    }

    public boolean implies(Permission perm)
    {
        return (perm instanceof ReadPermission) && (this.equals(perm) || this.equals("*"));
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (getClass() != obj.getClass())
            return false;
        ReadPermission other = (ReadPermission) obj;
        if (locator == null) {
            if (other.locator != null)
                return false;
        } else if (!locator.equals(other.locator))
            return false;
        return true;
    }

    public int hashCode()
    {
        final int prime = 31;
        int result = prime + ((locator == null) ? 0 : locator.hashCode());
        return result;
    }

    public String getActions()
    {
        return "";
    }
    
    private String locator;
    private static final long serialVersionUID = 3887436671296398427L;
}
