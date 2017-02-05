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

import org.davic.net.Locator;
import java.util.BitSet;

public class AppIcon {
    public AppIcon(Locator locator, BitSet flags) {
        this.locator = locator;
        this.flags = flags;
    }

    public Locator getLocator() {
        return locator;
    }

    public BitSet getIconFlags() {
        return flags;
    }

    private Locator locator;
    private BitSet flags;
}
