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

package javax.tv.service.selection;

import java.security.Permission;
import java.security.BasicPermission;

public final class ServiceContextPermission extends BasicPermission {
    public ServiceContextPermission(String name, String actions)
    {
        super(name);

        this.name = name;
        this.actions = actions;
    }

    public boolean implies(Permission perm)
    {
        if (!(perm instanceof ServiceContextPermission))
            return false;
        if (!perm.getActions().equals(actions) && !actions.equals("*"))
            return false;

        ServiceContextPermission scperm = (ServiceContextPermission) perm;

        if (!scperm.name.equals(name) && !name.equals("*"))
            return false;

        return true;
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (!super.equals(obj))
            return false;
        if (getClass() != obj.getClass())
            return false;
        ServiceContextPermission other = (ServiceContextPermission) obj;
        if (actions == null) {
            if (other.actions != null)
                return false;
        } else if (!actions.equals(other.actions))
            return false;
        if (name == null) {
            if (other.name != null)
                return false;
        } else if (!name.equals(other.name))
            return false;
        return true;
    }

    public int hashCode()
    {
        final int prime = 31;
        int result = super.hashCode();
        result = prime * result + ((actions == null) ? 0 : actions.hashCode());
        result = prime * result + ((name == null) ? 0 : name.hashCode());
        return result;
    }

    public String getActions()
    {
        return actions;
    }

    private static final long serialVersionUID = -649186793666419247L;
    private String name;
    private String actions;
}
