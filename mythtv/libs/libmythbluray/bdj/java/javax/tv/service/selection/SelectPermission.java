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
import javax.tv.locator.Locator;
import java.io.Serializable;

public final class SelectPermission extends Permission implements Serializable {
    public SelectPermission(Locator locator, String actions)
    {
        super(locator.toExternalForm());

        if (!actions.equals("own") && !actions.equals("*"))
            throw new IllegalArgumentException();

        this.locator = locator.toExternalForm();
        this.actions = actions;
    }

    public SelectPermission(String locator, String actions)
    {
        super(locator);

        if (!actions.equals("own") && !actions.equals("*"))
            throw new IllegalArgumentException();

        this.locator = locator;
        this.actions = actions;
    }

    public boolean implies(Permission perm)
    {
        if (!(perm instanceof SelectPermission))
            return false;
        if (!perm.getActions().equals(actions) && !actions.equals("*"))
            return false;

        SelectPermission sperm = (SelectPermission) perm;

        if (!sperm.locator.equals(locator) && !locator.equals("*"))
            return false;

        return true;
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (getClass() != obj.getClass())
            return false;
        SelectPermission other = (SelectPermission) obj;
        if (actions == null) {
            if (other.actions != null)
                return false;
        } else if (!actions.equals(other.actions))
            return false;
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
        int result = 1;
        result = prime * result + ((actions == null) ? 0 : actions.hashCode());
        result = prime * result + ((locator == null) ? 0 : locator.hashCode());
        return result;
    }

    public String getActions()
    {
        return actions;
    }

    private String actions;
    private String locator;
    private static final long serialVersionUID = 3418810478648506665L;
}
