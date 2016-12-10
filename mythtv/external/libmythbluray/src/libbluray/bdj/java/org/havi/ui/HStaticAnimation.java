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
    public HStaticAnimation() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public HStaticAnimation(Image[] imagesNormal, int delay, int playMode,
            int repeatCount, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public HStaticAnimation(Image[] imagesNormal, int delay, int playMode,
            int repeatCount) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public static void setDefaultLook(HAnimateLook hlook) {
        DefaultLook = hlook;
    }

    public static HAnimateLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public void start() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public void stop() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public boolean isAnimated() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
        return false;
    }

    public void setPosition(int position) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public int getPosition() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
        return 0;
    }

    public void setRepeatCount(int count) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public int getRepeatCount() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
        return 0;
    }

    public void setDelay(int count) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public int getDelay() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
        return 0;
    }

    public void setPlayMode(int mode) {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
    }

    public int getPlayMode() {
        org.videolan.Logger.unimplemented(HStaticAnimation.class.getName(), "");
        return 0;
    }

    private static HAnimateLook DefaultLook = null;

    private static final long serialVersionUID = -7320112528206101937L;
}
