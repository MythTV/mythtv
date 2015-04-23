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

public class HStaticAnimation extends HVisible implements HNoInputPreferred,
        HAnimateEffect {
    public HStaticAnimation()
    {
        throw new Error("Not implemented");
    }

    public HStaticAnimation(Image[] imagesNormal, int delay, int playMode,
            int repeatCount, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HStaticAnimation(Image[] imagesNormal, int delay, int playMode,
            int repeatCount)
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HAnimateLook hlook)
    {
        throw new Error("Not implemented");
    }

    public static HAnimateLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    public void start()
    {
        throw new Error("Not implemented");
    }

    public void stop()
    {
        throw new Error("Not implemented");
    }

    public boolean isAnimated()
    {
        throw new Error("Not implemented");
    }

    public void setPosition(int position)
    {
        throw new Error("Not implemented");
    }

    public int getPosition()
    {
        throw new Error("Not implemented");
    }

    public void setRepeatCount(int count)
    {
        throw new Error("Not implemented");
    }

    public int getRepeatCount()
    {
        throw new Error("Not implemented");
    }

    public void setDelay(int count)
    {
        throw new Error("Not implemented");
    }

    public int getDelay()
    {
        throw new Error("Not implemented");
    }

    public void setPlayMode(int mode)
    {
        throw new Error("Not implemented");
    }

    public int getPlayMode()
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = -7320112528206101937L;
}
