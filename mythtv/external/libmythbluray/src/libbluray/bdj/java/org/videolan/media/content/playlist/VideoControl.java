/*
 * This file is part of libbluray
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
import java.awt.Rectangle;

import org.dvb.media.VideoPresentationControl;
import org.havi.ui.HScreen;
import org.havi.ui.HScreenRectangle;
import org.havi.ui.HVideoConfiguration;
import org.videolan.StreamInfo;

public abstract class VideoControl extends StreamControl implements VideoPresentationControl {
    protected VideoControl(int plane) {
        this.plane = plane;
    }

    protected HScreenRectangle getNormalizedRectangle(Dimension dimension, Rectangle rectangle) {
        if ((dimension.width == 0) || (dimension.height == 0))
            return new HScreenRectangle(0, 0, 0, 0);
        float x = (float)rectangle.x / dimension.width;
        float y = (float)rectangle.y / dimension.height;
        float w = (float)rectangle.width / dimension.width;
        float h = (float)rectangle.height / dimension.height;
        return new HScreenRectangle(x, y, w, h);
    }

    protected Rectangle getRectangle(Dimension dimension, HScreenRectangle rectangle) {
        int x = (int)(rectangle.x * dimension.width);
        int y = (int)(rectangle.y * dimension.height);
        int w = (int)(rectangle.width * dimension.width);
        int h = (int)(rectangle.height * dimension.height);
        return new Rectangle(x, y, w, h);
    }

    protected float getPositionOnScreen(float pos) {
        if (pos < 0.0f)
            return 0.0f;
        if (pos > 1.0f)
            return 1.0f;
        return pos;
    }

    protected HScreenRectangle getRectangleOnScreen(HScreenRectangle rectangle) {
        float x1 = getPositionOnScreen(rectangle.x);
        float y1 = getPositionOnScreen(rectangle.y);
        float x2 = getPositionOnScreen(rectangle.x + rectangle.width);
        float y2 = getPositionOnScreen(rectangle.y + rectangle.height);
        return new HScreenRectangle(x1, y1, x2 - x1, y2 - y1);
    }

    protected Dimension getScreenSize() {
        HVideoConfiguration hvc = HScreen.getDefaultHScreen().getDefaultHVideoDevice().getCurrentConfiguration();
        return hvc.getPixelResolution();
    }

    protected void setVideoArea(HScreenRectangle rectangle) {
        org.videolan.Logger.unimplemented("VideoControl", "setVideoArea");
        dstArea = rectangle;
        // TODO
    }

    public Dimension getInputVideoSize() {
        StreamInfo stream = getCurrentStream();
        if (stream == null)
            return new Dimension(0, 0);
        return stream.getVideoSize();
    }

    public Dimension getVideoSize() {
        Rectangle dr = getRectangle(getScreenSize(), dstArea);
        return new Dimension(dr.width, dr.height);
    }

    public HScreenRectangle getActiveVideoArea() {
        return new HScreenRectangle(dstArea.x, dstArea.y, dstArea.width, dstArea.height);
    }

    public HScreenRectangle getActiveVideoAreaOnScreen() {
        return getRectangleOnScreen(dstArea);
    }

    public HScreenRectangle getTotalVideoArea() {
        return getActiveVideoArea();
    }

    public HScreenRectangle getTotalVideoAreaOnScreen() {
        return getActiveVideoAreaOnScreen();
    }

    public boolean supportsClipping() {
        return true;
    }

    public Rectangle getClipRegion() {
        return getRectangle(getVideoSize(), srcArea);
    }

    public Rectangle setClipRegion(Rectangle clipRect) {
        Dimension vd = getInputVideoSize();
        if ((vd.width == 0) || (vd.height == 0))
            return new Rectangle(0, 0);
        srcArea = getRectangleOnScreen(getNormalizedRectangle(vd, clipRect));

        //TODO
        org.videolan.Logger.unimplemented("VideoControl", "setClipRegion");

        return getRectangle(vd, srcArea);
    }

    public float[] supportsArbitraryHorizontalScaling() {
        return new float[] { 0.001f, 4.0f };
    }

    public float[] supportsArbitraryVerticalScaling() {
        return new float[] { 0.001f, 4.0f };
    }

    public float[] getHorizontalScalingFactors() {
        org.videolan.Logger.unimplemented("VideoControl", "getHorizontalScalingFactors");
        return null;
    }

    public float[] getVerticalScalingFactors() {
        org.videolan.Logger.unimplemented("VideoControl", "getVerticalScalingFactors");
        return null;
    }

    public byte getPositioningCapability() {
        return POS_CAP_FULL;
    }

    public Component getControlComponent() {
        org.videolan.Logger.unimplemented("VideoControl", "getControlComponent");
        return null;
    }

    private int plane = 0;
    private HScreenRectangle srcArea = new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f);
    private HScreenRectangle dstArea = new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f);
}
