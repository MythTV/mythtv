/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.awt.Point;

public class HImageEffectMatte implements HMatte, HAnimateEffect {

    private int delay = 1;
    private boolean isAnimated = false;
    private Image[] matteData = null;
    private int mode = PLAY_ALTERNATING;
    private Point[] offsets = null;
    private int position = 0;
    private int repeatCount = -1;

    public HImageEffectMatte() {
    }

    public HImageEffectMatte(Image[] data) {
        this();
        setMatteData(data);
    }

    public void setMatteData(Image[] data) {
        if (data == null) {
            matteData = null;
            offsets = null;
            return;
        }
        if (data.length == 0) {
            org.videolan.Logger.getLogger("HImageEffectMatte").error("setMatteData: empty image array");
            throw new IllegalArgumentException("empty image array");
        }
        matteData = data;

        offsets = new Point[matteData.length];
        for (int i = 0; i < matteData.length; i++)
            offsets[i] = new Point(0, 0);
    }

    public Image[] getMatteData() {
        return matteData;
    }

    public void setOffset(Point p, int index) {
        if (p == null) {
            org.videolan.Logger.getLogger("HImageEffectMatte").error("setOffset(): no point");
            throw new IllegalArgumentException("setOffset(): point is null");
        }

        if (matteData == null || index < 0 || index >= matteData.length) {
            org.videolan.Logger.getLogger("HImageEffectMatte").error("setOffset(): invalid index");
            throw new IndexOutOfBoundsException("setOffset(): invalid index");
        }

        offsets[index] = p;
    }

    public Point getOffset(int index) {
        return offsets[index];
    }

    public void start() {
        isAnimated = true;
    }

    public void stop() {
        isAnimated = false;
    }

    public boolean isAnimated() {
        return isAnimated;
    }

    public void setPosition(int position) {
        if (matteData == null)
            return;
        if (position < 0)
            position = 0;
        if (position >= matteData.length) {
            position = matteData.length - 1;
        }
        this.position = position;
    }

    public int getPosition() {
        return position;
    }

    public void setRepeatCount(int count) {
        if (count <= 0 && count != -1) {
            org.videolan.Logger.getLogger("HImageEffectMatte").error("setRepeatCount(): invalid count");
            throw new IllegalArgumentException("setRepeatCount(): invalid count");
        }

        repeatCount = count;
    }

    public int getRepeatCount() {
        return repeatCount;
    }

    public void setDelay(int count) {
        if (count < 1)
            count = 1;
        delay = count;
    }

    public int getDelay() {
        return delay;
    }

    public void setPlayMode(int mode) {
        if (mode != PLAY_REPEATING && mode != PLAY_ALTERNATING) {
            org.videolan.Logger.getLogger("HImageEffectMatte").error("setPlayMode(): invalid mode");
            throw new IllegalArgumentException("setPlayMode(): invalid mode");
        }
        this.mode = mode;
    }

    public int getPlayMode() {
        return mode;
    }
}
