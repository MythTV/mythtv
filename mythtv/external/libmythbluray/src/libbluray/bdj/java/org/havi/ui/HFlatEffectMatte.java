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

public class HFlatEffectMatte implements HMatte, HAnimateEffect {

    private int delay = 1;
    private boolean isAnimated = false;
    private float[] matteData = null;
    private int mode = PLAY_REPEATING;
    private int position = 0;
    private int repeatCount = -1;

    public HFlatEffectMatte() {
    }

    public HFlatEffectMatte(float[] data) {
        this();
        setMatteData(data);
    }

    public void setMatteData(float[] data) {
        if (data == null) {
            matteData = null;
            return;
        }
        if (data.length == 0) {
            org.videolan.Logger.getLogger("HFlatEffectMatte").error("setMatteData: empty data");
            throw new IllegalArgumentException("empty data");
        }
        matteData = data;
    }

    public float[] getMatteData() {
        return matteData;
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
            org.videolan.Logger.getLogger("HFlatEffectMatte").error("setPlayMode(): invalid mode");
            throw new IllegalArgumentException("setPlayMode(): invalid mode");
        }
        this.mode = mode;
    }

    public int getPlayMode() {
        return mode;
    }
}
