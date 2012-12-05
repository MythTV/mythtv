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

package org.dvb.application;

import java.security.BasicPermission;
import java.security.Permission;

public final class AppsControlPermission extends BasicPermission {
    public AppsControlPermission()
    {
        super("toto");
    }

    public AppsControlPermission(String name, String actions)
    {
        super(name);
    }

    public String getActions()
    {
        return actions;
    }

    public boolean implies(Permission perm)
    {
        return perm instanceof AppsControlPermission;
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (!super.equals(obj))
            return false;
        if (getClass() != obj.getClass())
            return false;
        AppsControlPermission other = (AppsControlPermission) obj;
        if (actions == null) {
            if (other.actions != null)
                return false;
        } else if (!actions.equals(other.actions))
            return false;
        return true;
    }

    public int hashCode()
    {
        final int prime = 31;
        int result = super.hashCode();
        result = prime * result + ((actions == null) ? 0 : actions.hashCode());
        return result;
    }

    private String actions;
    private static final long serialVersionUID = 4003360685455291075L;
}
