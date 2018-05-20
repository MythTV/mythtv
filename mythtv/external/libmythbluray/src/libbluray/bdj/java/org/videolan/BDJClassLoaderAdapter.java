/*
 * This file is part of libbluray
 * Copyright (C) 2017  VideoLAN
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

package org.videolan;

import java.util.Map;

public interface BDJClassLoaderAdapter {

    /*
     * Modify Xlet class path at run time
     */

     /* classes that should be hidden from Xlet.
      * Can be used to hide profile 5 or 6 classes when
      * running as profile 2 player.
      */
    public abstract Map getHideClasses();

    /* Additional bootstrap classes (highest priority) */
    public abstract Map getBootClasses();

    /* normal classes (lowest priority) */
    public abstract Map getXletClasses();
}
