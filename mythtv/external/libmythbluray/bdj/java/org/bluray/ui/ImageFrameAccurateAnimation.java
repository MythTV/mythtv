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
import java.awt.Image;

public class ImageFrameAccurateAnimation extends FrameAccurateAnimation {
    public static ImageFrameAccurateAnimation getInstance(Image[] images,
            Dimension size, AnimationParameters params, int playmode)
            throws NullPointerException, IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    private ImageFrameAccurateAnimation()
    {

    }

    @Deprecated
    public AnimationParameters getAnimationParameters()
    {
        throw new Error("Not implemented");
    }

    public Image[] getImages()
    {
        throw new Error("Not implemented");
    }

    public int getPlayMode()
    {
        throw new Error("Not implemented");
    }

    public int getPosition()
    {
        throw new Error("Not implemented");
    }

    public void prepareImages()
    {
        throw new Error("Not implemented");
    }

    public void setPlayMode(int mode) throws IllegalArgumentException
    {
        throw new Error("Not implemented");
    }

    public void setPosition(int position)
    {
        throw new Error("Not implemented");
    }

    protected void destroyImp()
    {
        throw new Error("Not implemented");
    }

    protected void startImp()
    {
        throw new Error("Not implemented");
    }

    public void paint(Graphics g)
    {
        throw new Error("Not implemented");
    }

    public static final int PLAY_REPEATING = 1;
    public static final int PLAY_ALTERNATING = 2;
    public static final int PLAY_ONCE = 3;

    private static final long serialVersionUID = 2691302238670178111L;
}
