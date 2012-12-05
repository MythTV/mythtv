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

import java.awt.Color;
import java.awt.Font;

public class HStaticText extends HVisible implements HNoInputPreferred {
    public HStaticText()
    {
        throw new Error("Not implemented");
    }

    public HStaticText(String textNormal, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HStaticText(String textNormal, int x, int y, int width, int height,
            Font font, Color foreground, Color background,
            HTextLayoutManager tlm)
    {
        throw new Error("Not implemented");
    }

    public HStaticText(String textNormal)
    {
        throw new Error("Not implemented");
    }

    public HStaticText(String textNormal, Font font, Color foreground,
            Color background, HTextLayoutManager tlm)
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HTextLook hlook)
    {
        throw new Error("Not implemented");
    }

    public static HTextLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = 4352450387189482885L;
}
