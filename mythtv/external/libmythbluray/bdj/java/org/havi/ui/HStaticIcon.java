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

package org.havi.ui;

import java.awt.Image;

public class HStaticIcon extends HVisible implements HNoInputPreferred {
    public HStaticIcon()
    {
        throw new Error("Not implemented");
    }

    public HStaticIcon(Image imageNormal, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HStaticIcon(Image imageNormal)
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HGraphicLook hlook)
    {
        throw new Error("Not implemented");
    }

    public static HGraphicLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = 2015589998794748072L;
}
