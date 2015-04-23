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

package javax.tv.graphics;

import java.awt.Color;

public class AlphaColor extends Color {
    public AlphaColor(float r, float g, float b, float a)
    {
        super(r, g, b, a);
    }

    public AlphaColor(int r, int g, int b, int a)
    {
        super(r, g, b, a);
    }

    public AlphaColor(int rgba, boolean hasAlpha)
    {
        super(rgba, hasAlpha);
    }

    public AlphaColor(Color c)
    {
        super(c.getRGB());
    }

    private static final long serialVersionUID = -3466072971590811211L;
}
