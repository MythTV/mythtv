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

public interface HAnimateEffect {
    public void start();

    public void stop();

    public boolean isAnimated();

    public void setPosition(int position);

    public int getPosition();

    public void setRepeatCount(int count);

    public int getRepeatCount();

    public void setDelay(int count);

    public int getDelay();

    public void setPlayMode(int mode);

    public int getPlayMode();

    public static final int PLAY_REPEATING = 1;
    public static final int PLAY_ALTERNATING = 2;
    public static final int REPEAT_INFINITE = -1;
}
