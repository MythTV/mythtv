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

import org.videolan.Arrays;

public class AnimationParameters {
    public String toString() {
        return "AnimationParameters [faaTimer=" + faaTimer +
               ", lockedToVideo=" + lockedToVideo +
               ", repeatCount=" + Arrays.toString(repeatCount) +
               ", scaleFactor=" + scaleFactor +
               ", threadPriority=" + threadPriority + "]";
    }

    public AnimationParameters() {
    }

    AnimationParameters(AnimationParameters p) {
        if (p.faaTimer != null) {
            faaTimer = new FrameAccurateAnimationTimer(p.faaTimer);
        }
        lockedToVideo = p.lockedToVideo;
        if (p.repeatCount != null) {
            repeatCount = (int[])p.repeatCount.clone();
        }
        scaleFactor = p.scaleFactor;
        threadPriority = p.threadPriority;
    }

    public FrameAccurateAnimationTimer faaTimer = null;
    public boolean lockedToVideo = false;  /* frame rate locked to video frame rate */
    public int[] repeatCount = null;       /* repeat each frame repeatCount times */
    public int scaleFactor = 1;            /* scale factor when painting */
    public int threadPriority = 5;
}
