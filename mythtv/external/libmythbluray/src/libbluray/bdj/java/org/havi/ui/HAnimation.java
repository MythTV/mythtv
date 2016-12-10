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
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

import org.videolan.Logger;

public class HAnimation extends HStaticAnimation implements HNavigable {
    public HAnimation() {
        this(null, 1, 1, -1);
    }

    public HAnimation(Image[] images, int delay, int playMode, int repeatCount) {
        this(images, null, delay, playMode, repeatCount, 0, 0, 0, 0);
    }

    public HAnimation(Image[] images, int delay, int playMode, int repeatCount,
                      int x, int y, int width, int height) {
        this(images, null, delay, playMode, repeatCount, x, y, width, height);
    }

    public HAnimation(Image[] imagesNormal, Image[] imagesFocused, int delay,
                      int playMode, int repeatCount) {
        this(imagesNormal, imagesFocused, delay, playMode, repeatCount, 0, 0, 0, 0);
    }

    public HAnimation(Image[] imagesNormal, Image[] imagesFocused, int delay,
            int playMode, int repeatCount, int x, int y, int width, int height) {

        super(imagesNormal, delay, playMode, repeatCount, x, y, width, height);

        logger.unimplemented("");
    }

    public static void setDefaultLook(HAnimateLook hlook) {
        DefaultLook = hlook;
    }

    public static HAnimateLook getDefaultLook() {
        if (DefaultLook == null)
            logger.unimplemented("getDefaultLook");
        return DefaultLook;
    }

    public void setMove(int keyCode, HNavigable target) {
        logger.unimplemented("setMove");
    }

    public HNavigable getMove(int keyCode) {
        logger.unimplemented("getMove");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        logger.unimplemented("setFocusTravelsal");
    }

    public boolean isSelected() {
        logger.unimplemented("");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        logger.unimplemented("");
    }

    public void setLoseFocusSound(HSound sound) {
        logger.unimplemented("");
    }

    public HSound getGainFocusSound() {
        logger.unimplemented("");
        return null;
    }

    public HSound getLoseFocusSound() {
        logger.unimplemented("");
        return null;
    }

    public void addHFocusListener(HFocusListener listener) {
        logger.unimplemented("");
    }

    public void removeHFocusListener(HFocusListener listener) {
        logger.unimplemented("");
    }

    public int[] getNavigationKeys() {
        logger.unimplemented("");
        return null;
    }

    public void processHFocusEvent(HFocusEvent event) {
        logger.unimplemented("");
    }

    private static HAnimateLook DefaultLook = null;

    private static final Logger logger = Logger.getLogger(HAnimation.class.getName());

    private static final long serialVersionUID = 4460392782940525395L;
}
