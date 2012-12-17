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

package org.videolan.media.content.playlist;

import java.awt.Component;
import java.awt.Dimension;

import org.dvb.media.VideoFormatControl;
import org.dvb.media.VideoFormatListener;
import org.dvb.media.VideoTransformation;
import org.havi.ui.HScreen;
import org.havi.ui.HVideoConfiguration;
import org.videolan.TIClip;

public class VideoFormatControlImpl implements VideoFormatControl {
    protected VideoFormatControlImpl(Handler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public int getAspectRatio() {
        TIClip ci = player.getCurrentClipInfo();
        if ((ci == null) ||(ci.getVideoStreamCount() <= 0))
            return ASPECT_RATIO_UNKNOWN;
        Dimension aspect = ci.getVideoStreams()[0].getVideoAspectRatio();
        if ((aspect.width == 4) && (aspect.height == 3))
            return ASPECT_RATIO_4_3;
        if ((aspect.width == 16) && (aspect.height == 9))
            return ASPECT_RATIO_16_9;
        return ASPECT_RATIO_UNKNOWN;
    }

    public int getActiveFormatDefinition() {
        return AFD_NOT_PRESENT;
    }

    public int getDecoderFormatConversion() {
        return DFC_PROCESSING_NONE; // FIXME: get actual DFC
    }

    public VideoTransformation getVideoTransformation(int dfc) {
        return null; // TODO: implement
    }

    public int getDisplayAspectRatio() {
        HVideoConfiguration hvc = HScreen.getDefaultHScreen().getDefaultHVideoDevice().getCurrentConfiguration();
        Dimension resolution = hvc.getPixelResolution();
        if (resolution.width == 720)
            return DAR_4_3;
        return DAR_16_9;
    }

    public boolean isPlatform() {
        return dfc == DFC_PLATFORM;
    }

    public void addVideoFormatListener(VideoFormatListener listener) {
        // TODO: implement
    }

    public void removeVideoFormatListener(VideoFormatListener listener) {
        // TODO: implement
    }

    private Handler player;
    private int dfc = DFC_PROCESSING_NONE;
}
