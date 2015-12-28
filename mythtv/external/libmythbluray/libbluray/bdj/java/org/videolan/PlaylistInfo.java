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

package org.videolan;

public class PlaylistInfo {
    public PlaylistInfo(int playlist, long duration, int angles, TIMark[] marks, TIClip[] clips) {
        this.playlist = playlist;
        this.duration = duration;
        this.angles = angles;
        this.marks = marks;
        this.clips = clips;
    }

    public int getPlaylist() {
        return playlist;
    }

    public long getDuration() {
        return duration;
    }

    public int getMarkCount() {
        return marks.length;
    }

    public TIMark[] getMarks() {
        return marks;
    }

    public int getClipCount() {
        return clips.length;
    }

    public TIClip[] getClips() {
        return clips;
    }

    public int getAngleCount() {
        return angles;
    }

    private int playlist;
    private long duration;
    private int angles;
    private TIMark[] marks;
    private TIClip[] clips;
}