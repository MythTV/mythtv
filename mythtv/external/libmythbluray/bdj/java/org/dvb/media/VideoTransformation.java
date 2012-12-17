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

package org.dvb.media;

import java.awt.Rectangle;
import org.havi.ui.HScreenPoint;

public class VideoTransformation {
    public VideoTransformation() {
        hscaling = 1.0f;
        vscaling = 1.0f;
        position = new HScreenPoint(0.0f, 0.0f);
    }

    public VideoTransformation(Rectangle clipRect,
            float horizontalScalingFactor, float verticalScalingFactor,
            HScreenPoint location) {
        if (clipRect != null)
                clip = new Rectangle(clipRect);
        hscaling = horizontalScalingFactor;
        vscaling = verticalScalingFactor;
        position = new HScreenPoint(location.x, location.y);
    }

    public void setClipRegion(Rectangle clipRect) {
        clip = (clipRect != null) ? new Rectangle(clipRect) : null;
    }

    public Rectangle getClipRegion() {
        return (!isPanAndScan() && (clip != null)) ? new Rectangle(clip) : null;
    }

    public void setScalingFactors(float horizontalScalingFactor, float verticalScalingFactor) {
        hscaling = horizontalScalingFactor;
        vscaling = verticalScalingFactor;
    }

    public float[] getScalingFactors() {
        return new float[] { hscaling, vscaling };
    }

    public void setVideoPosition(HScreenPoint location) {
        position = new HScreenPoint(location.x, location.y);
    }

    public HScreenPoint getVideoPosition() {
        return new HScreenPoint(position.x, position.y);
    }

    public boolean isPanAndScan() {
        return false;
    }

    private Rectangle clip = null;
    private float hscaling, vscaling;
    private HScreenPoint position;
}
