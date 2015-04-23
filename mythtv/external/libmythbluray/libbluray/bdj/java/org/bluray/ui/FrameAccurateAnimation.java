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

import java.awt.Component;
import java.awt.Graphics;

public abstract class FrameAccurateAnimation extends Component {
    public static final float getDefaultFrameRate()
    {
        throw new Error("Not implemented");
    }

    public static boolean setDefaultFrameRate(float framerate)
    {
        throw new Error("Not implemented");
    }

    public FrameAccurateAnimation()
    {
        throw new Error("Not implemented");
    }

    public synchronized void destroy()
    {
        throw new Error("Not implemented");
    }

    public long getCompletedFrameCount()
    {
        throw new Error("Not implemented");
    }

    public float getFrameRate()
    {
        throw new Error("Not implemented");
    }

    public Graphics getGraphics()
    {
        throw new Error("Not implemented");
    }

    public int[] getRepeatCounts()
    {
        throw new Error("Not implemented");
    }

    public int getThreadPriority()
    {
        throw new Error("Not implemented");
    }

    public synchronized boolean isAnimated()
    {
        throw new Error("Not implemented");
    }

    public void paint(Graphics g)
    {
        throw new Error("Not implemented");
    }

    public synchronized void resetStartStopTime(
            FrameAccurateAnimationTimer newTimer)
    {
        throw new Error("Not implemented");
    }

    public void setBounds(int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public void setLocation(int x, int y)
    {
        throw new Error("Not implemented");
    }

    public void setThreadPriority(int p)
    {
        throw new Error("Not implemented");
    }

    public synchronized void start()
    {
        throw new Error("Not implemented");
    }

    public synchronized void stop()
    {
        throw new Error("Not implemented");
    }

    public String toString()
    {
        return "FrameAccurateAnimation";
    }

    public static final float FRAME_RATE_23_976 = 23.976F;
    public static final float FRAME_RATE_24 = 24.0F;
    public static final float FRAME_RATE_25 = 25.0F;
    public static final float FRAME_RATE_29_97 = 29.969999F;
    public static final float FRAME_RATE_50 = 50.0F;
    public static final float FRAME_RATE_59_94 = 59.939999F;

    private static final long serialVersionUID = 76982966057159330L;
}
