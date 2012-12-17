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

package org.bluray.media;

import javax.media.Control;

public abstract interface PlaybackControl extends Control {
    public abstract void addPlaybackControlListener(PlaybackListener listener);

    public abstract void removePlaybackControlListener(PlaybackListener listener);

    public abstract void skipToMark(int mark) throws IllegalArgumentException;

    public abstract boolean skipToNextMark(int mark)
            throws IllegalArgumentException;

    public abstract boolean skipToPreviousMark(int mark)
            throws IllegalArgumentException;

    public abstract void skipToPlayItem(int item)
            throws IllegalArgumentException;

    public static final int ENTRYMARK = 1;
    public static final int LINKPOINT = 2;
}
