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

public class TIClip {
    public TIClip(int index, StreamInfo[] videoStreams, StreamInfo[] audioStreams, StreamInfo[] pgStreams,
            StreamInfo[] igStreams, StreamInfo[] secVideoStreams, StreamInfo[] secAudioStreams) {
        this.index = index;
        this.videoStreams = videoStreams;
        this.audioStreams = audioStreams;
        this.pgStreams = pgStreams;
        this.igStreams = igStreams;
        this.secVideoStreams = secVideoStreams;
        this.secAudioStreams = secAudioStreams;
    }

    public int getIndex() {
        return index;
    }

    public int getVideoStreamCount() {
        return videoStreams.length;
    }

    public StreamInfo[] getVideoStreams() {
        return videoStreams;
    }

    public int getAudioStreamCount() {
        return audioStreams.length;
    }

    public StreamInfo[] getAudioStreams() {
        return audioStreams;
    }

    public int getPgStreamCount() {
        return pgStreams.length;
    }

    public StreamInfo[] getPgStreams() {
        return pgStreams;
    }

    public int getIgStreamCount() {
        return igStreams.length;
    }

    public StreamInfo[] getIgStreams() {
        return igStreams;
    }

    public int getSecVideoStreamCount() {
        return secVideoStreams.length;
    }

    public StreamInfo[] getSecVideoStreams() {
        return secVideoStreams;
    }

    public int getSecAudioStreamCount() {
        return secAudioStreams.length;
    }

    public StreamInfo[] getSecAudioStreams() {
        return secAudioStreams;
    }

    private int index;
    private StreamInfo[] videoStreams = null;
    private StreamInfo[] audioStreams = null;
    private StreamInfo[] pgStreams = null;
    private StreamInfo[] igStreams = null;
    private StreamInfo[] secVideoStreams = null;
    private StreamInfo[] secAudioStreams = null;
}
