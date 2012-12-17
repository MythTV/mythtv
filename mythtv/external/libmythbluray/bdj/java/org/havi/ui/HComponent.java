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

import java.awt.AWTEvent;
import java.awt.Component;
import org.dvb.ui.TestOpacity;

public abstract class HComponent extends Component implements HMatteLayer,
        TestOpacity {
    public HComponent()
    {
        throw new Error("Not implemented");
    }

    public HComponent(int x, int y, int width, int height)
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

    public void setEnabled(boolean b)
    {
        throw new Error("Not implemented");
    }

    public boolean isEnabled()
    {
        throw new Error("Not implemented");
    }

    protected void processEvent(AWTEvent event)
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = -4115249517434074428L;
}
