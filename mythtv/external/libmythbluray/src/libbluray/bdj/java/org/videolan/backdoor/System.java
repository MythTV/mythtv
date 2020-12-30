/*
 * This file is part of libbluray
 * Copyright (C) 2020  Petri Hintukainen <phintuka@users.sourceforge.net>
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

/*
 * Public package, exported to Xlets.
 */

package org.videolan.backdoor;

public class System {

    private static final Object syncBarrier = new Object();

    public static long currentTimeMillisSynced() {
        synchronized (syncBarrier) {
            return java.lang.System.currentTimeMillis();
        }
    }
}
