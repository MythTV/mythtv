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

package org.bluray.ui;

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import javax.media.Time;

public class SyncFrameAccurateAnimation extends FrameAccurateAnimation {
    public static SyncFrameAccurateAnimation getInstance(Dimension size,
            int numFrames, AnimationParameters params)
            throws NullPointerException, IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    public boolean finishDrawing(long frameNumber)
            throws IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    public boolean finishDrawing(long frameNumber, Rectangle[] updateArea)
            throws IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    public Time getAnimationFrameTime(long animationFrame)
            throws IllegalStateException
    {
        throw new Error("Not implemented");
    }

    public void paint(Graphics graphics)
    {
        throw new Error("Not implemented");
    }

    public void setBounds(int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public Graphics2D startDrawing(long frameNumber)
            throws IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = -1340138671201204543L;
}
