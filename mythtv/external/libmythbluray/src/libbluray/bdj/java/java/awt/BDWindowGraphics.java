/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package java.awt;

import java.awt.image.ImageObserver;

public class BDWindowGraphics extends BDGraphics {
    private BDRootWindow window;

    BDWindowGraphics(BDWindowGraphics g) {
        super(g);
        window = g.window;
    }

    public BDWindowGraphics(BDRootWindow window) {
        super(window);
        this.window = window;
    }

    public Graphics create() {
        return new BDWindowGraphics(this);
    }

    public void clearRect(int x, int y, int w, int h) {
        if (window == null) return;
        synchronized (window) {
            super.clearRect(x, y, w, h);
            window.notifyChanged();
        }
    }

    public void fillRect(int x, int y, int w, int h) {
        if (window == null) return;
        synchronized (window) {
            super.fillRect(x, y, w, h);
            window.notifyChanged();
        }
    }

    public void drawRect(int x, int y, int w, int h) {
        if (window == null) return;
        synchronized (window) {
            super.drawRect(x, y, w, h);
            window.notifyChanged();
        }
    }

    public void drawLine(int x1, int y1, int x2, int y2) {
        if (window == null) return;
        synchronized (window) {
            super.drawLine(x1, y1, x2, y2);
            window.notifyChanged();
        }
    }

    public void copyArea(int x, int y, int w, int h, int dx, int dy) {
        if (window == null) return;
        synchronized (window) {
            super.copyArea(x, y, w, h, dx, dy);
            window.notifyChanged();
        }
    }

    public void drawPolyline(int xPoints[], int yPoints[], int nPoints) {
        if (window == null) return;
        synchronized (window) {
            super.drawPolyline(xPoints, yPoints, nPoints);
            window.notifyChanged();
        }
    }

    public void drawPolygon(int xPoints[], int yPoints[], int nPoints) {
        if (window == null) return;
        synchronized (window) {
            super.drawPolygon(xPoints, yPoints, nPoints);
            window.notifyChanged();
        }
    }

    public void fillPolygon(int xPoints[], int yPoints[], int nPoints) {
        if (window == null) return;
        synchronized (window) {
            super.fillPolygon(xPoints, yPoints, nPoints);
            window.notifyChanged();
        }
    }

    public void drawOval(int x, int y, int w, int h) {
        if (window == null) return;
        synchronized (window) {
            super.drawOval(x, y, w, h);
            window.notifyChanged();
        }
    }

    public void fillOval(int x, int y, int w, int h) {
        if (window == null) return;
        synchronized (window) {
            super.fillOval(x, y, w, h);
            window.notifyChanged();
        }
    }

    public void drawArc(int x, int y, int w, int h, int startAngle, int endAngle) {
        if (window == null) return;
        synchronized (window) {
            super.drawArc(x, y, w, h, startAngle, endAngle);
            window.notifyChanged();
        }
    }

    public void fillArc(int x, int y, int w, int h, int startAngle, int endAngle) {
        if (window == null) return;
        synchronized (window) {
            super.fillArc(x, y, w, h, startAngle, endAngle);
            window.notifyChanged();
        }
    }

    public void drawRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight) {
        if (window == null) return;
        synchronized (window) {
            super.drawRoundRect(x, y, w, h, arcWidth, arcHeight);
            window.notifyChanged();
        }
    }

    public void fillRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight) {
        if (window == null) return;
        synchronized (window) {
            super.fillRoundRect(x, y, w, h, arcWidth, arcHeight);
            window.notifyChanged();
        }
    }

    protected void drawStringN(long ftFace, String string, int x, int y, int rgb) {
        if (window == null) return;
        synchronized (window) {
            super.drawStringN(ftFace, string, x, y, rgb);
            window.notifyChanged();
        }
    }

    public void dispose() {
        super.dispose();
        window = null;
    }

    public boolean drawImageN(Image img,
        int dx, int dy, int dw, int dh,
        int sx, int sy, int sw, int sh,
        boolean flipX, boolean flipY,
        Color bg, ImageObserver observer) {

        if (window == null) return true;

        synchronized (window) {
            boolean complete = super.drawImageN(
                img, dx, dy, dw, dh, sx, sy, sw, sh,
                flipX, flipY,
                bg, observer);
            if (complete) {
                window.notifyChanged();
            }
            return complete;
        }
    }
}
