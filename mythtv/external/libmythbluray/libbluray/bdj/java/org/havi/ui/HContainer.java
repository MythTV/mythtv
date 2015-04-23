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

import java.awt.Component;
import java.awt.Container;
import org.dvb.ui.TestOpacity;

public class HContainer extends Container implements HMatteLayer,
        HComponentOrdering, TestOpacity {
    public HContainer()
    {
        throw new Error("Not implemented");
    }

    public HContainer(int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public void setMatte(HMatte m) throws HMatteException
    {
        throw new Error("Not implemented");
    }

    public HMatte getMatte()
    {
        throw new Error("Not implemented");
    }

    public boolean isDoubleBuffered()
    {
        throw new Error("Not implemented");
    }

    public boolean isOpaque()
    {
        throw new Error("Not implemented");
    }

    public Component addBefore(Component component, Component behind)
    {
        throw new Error("Not implemented");
    }

    public Component addAfter(Component component, Component front)
    {
        throw new Error("Not implemented");
    }

    public boolean popToFront(Component component)
    {
        throw new Error("Not implemented");
    }

    public boolean pushToBack(Component component)
    {
        throw new Error("Not implemented");
    }

    public boolean pop(Component component)
    {
        throw new Error("Not implemented");
    }

    public boolean push(Component component)
    {
        throw new Error("Not implemented");
    }

    public boolean popInFrontOf(Component move, Component behind)
    {
        throw new Error("Not implemented");
    }

    public boolean pushBehind(Component move, Component front)
    {
        throw new Error("Not implemented");
    }

    public void group()
    {
        throw new Error("Not implemented");
    }

    public void ungroup()
    {
        throw new Error("Not implemented");
    }

    public boolean isGrouped()
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = 263606166411114032L;
}
