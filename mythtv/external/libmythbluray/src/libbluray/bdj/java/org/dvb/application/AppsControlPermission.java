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
    public AppsControlPermission() {
        super("*");
    }

    public AppsControlPermission(String name, String actions) {
        super("*");
    }

    public String getActions() {
        // actions should be always null
        return null;
    }

    public boolean implies(Permission perm) {
        return perm instanceof AppsControlPermission;
    }

    public boolean equals(Object obj) {
        return obj instanceof AppsControlPermission;
    }

    public int hashCode() {
        return 0;
    }

    private static final long serialVersionUID = 4003360685455291075L;
}
