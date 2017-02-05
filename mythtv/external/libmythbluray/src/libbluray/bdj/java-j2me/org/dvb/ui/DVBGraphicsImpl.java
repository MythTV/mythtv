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

package org.dvb.ui;

import java.awt.AlphaComposite;
import java.awt.Color;
import java.awt.Composite;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GraphicsConfiguration;
import java.awt.Image;
import java.awt.Polygon;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.Stroke;
import java.awt.image.ImageObserver;
import java.text.AttributedCharacterIterator;

public class DVBGraphicsImpl extends DVBGraphics {

    private Graphics2D gfx;

    protected DVBGraphicsImpl(Graphics2D gfx)
    {
        this.gfx = gfx;
    }

    /*
     * Graphics methods
     */
    public void clearRect(int x, int y, int width, int height)
    {
        gfx.clearRect(x, y, width, height);
    }

    public void clipRect(int x, int y, int width, int height)
    {
        gfx.clipRect(x, y, width, height);
    }

    public void copyArea(int x, int y, int width, int height, int dx, int dy)
    {
        gfx.copyArea(x, y, width, height, dx, dy);
    }

    public Graphics create()
    {
        return gfx.create();
    }

    public Graphics create(int x, int y, int width, int height)
    {
        return gfx.create(x, y, width, height);
    }

    public void dispose()
    {
        gfx.dispose();
    }

    public void draw3DRect(int x, int y, int width, int height, boolean raised)
    {
        gfx.draw3DRect(x, y, width, height, raised);
    }

    public void drawArc(int x, int y, int width, int height, int startAngle,
            int arcAngle)
    {
        gfx.drawArc(x, y, width, height, startAngle, arcAngle);
    }

    public void drawBytes(byte[] data, int offset, int length, int x, int y)
    {
        gfx.drawBytes(data, offset, length, x, y);
    }

    public void drawChars(char[] data, int offset, int length, int x, int y)
    {
        gfx.drawChars(data, offset, length, x, y);
    }

    public boolean drawImage(Image img, int x, int y, Color bgcolor,
            ImageObserver observer)
    {
        return gfx.drawImage(img, x, y, bgcolor, observer);
    }

    public boolean drawImage(Image img, int x, int y, ImageObserver observer)
    {
        return gfx.drawImage(img, x, y, observer);
    }

    public boolean drawImage(Image img, int x, int y, int width, int height,
            Color bgcolor, ImageObserver observer)
    {
        return gfx.drawImage(img, x, y, width, height, bgcolor, observer);
    }

    public boolean drawImage(Image img, int x, int y, int width, int height,
            ImageObserver observer)
    {
        return gfx.drawImage(img, x, y, width, height, observer);
    }

    public boolean drawImage(Image img, int dx1, int dy1, int dx2, int dy2,
            int sx1, int sy1, int sx2, int sy2, Color bgcolor,
            ImageObserver observer)
    {
        return gfx.drawImage(img, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2,
                             bgcolor, observer);
    }

    public boolean drawImage(Image img, int dx1, int dy1, int dx2, int dy2,
            int sx1, int sy1, int sx2, int sy2, ImageObserver observer)
    {
        return gfx.drawImage(img, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2,
                             observer);
    }

    public void drawLine(int x1, int y1, int x2, int y2)
    {
        gfx.drawLine(x1, y1, x2, y2);
    }

    public void drawOval(int x, int y, int width, int height)
    {
        gfx.drawOval(x, y, width, height);
    }

    public void drawPolygon(int[] xPoints, int[] yPoints, int nPoints)
    {
        gfx.drawPolygon(xPoints, yPoints, nPoints);
    }

    public void drawPolygon(Polygon p)
    {
        gfx.drawPolygon(p);
    }

    public void drawPolyline(int[] xPoints, int[] yPoints, int nPoints)
    {
        gfx.drawPolyline(xPoints, yPoints, nPoints);
    }

    public void drawRect(int x, int y, int width, int height)
    {
        gfx.drawRect(x, y, width, height);
    }

    public void drawRoundRect(int x, int y, int width, int height,
            int arcWidth, int arcHeight)
    {
        gfx.drawRoundRect(x, y, width, height, arcWidth, arcHeight);
    }

    public void drawString(AttributedCharacterIterator iterator, int x, int y)
    {
        gfx.drawString(iterator, x, y);
    }

    public void drawString(String str, int x, int y)
    {
        gfx.drawString(str, x, y);
    }

    public void fill3DRect(int x, int y, int width, int height, boolean raised)
    {
        gfx.fill3DRect(x, y, width, height, raised);
    }

    public void fillArc(int x, int y, int width, int height, int startAngle,
            int arcAngle)
    {
        gfx.fillArc(x, y, width, height, startAngle, arcAngle);
    }

    public void fillOval(int x, int y, int width, int height)
    {
        gfx.fillOval(x, y, width, height);
    }

    public void fillPolygon(int[] xPoints, int[] yPoints, int nPoints)
    {
        gfx.fillPolygon(xPoints, yPoints, nPoints);
    }

    public void fillPolygon(Polygon p)
    {
        gfx.fillPolygon(p);
    }

    public void fillRect(int x, int y, int width, int height)
    {
        gfx.fillRect(x, y, width, height);
    }

    public void fillRoundRect(int x, int y, int width, int height,
            int arcWidth, int arcHeight)
    {
        gfx.fillRoundRect(x, y, width, height, arcWidth, arcHeight);
    }

    public void finalize()
    {
        gfx.finalize();
    }

    public Shape getClip()
    {
        return gfx.getClip();
    }

    public Rectangle getClipBounds()
    {
        return gfx.getClipBounds();
    }

    public Rectangle getClipBounds(Rectangle r)
    {
        return gfx.getClipBounds(r);
    }

    public Color getColor()
    {
        return gfx.getColor();
    }

    public Font getFont()
    {
        return gfx.getFont();
    }

    public FontMetrics getFontMetrics()
    {
        return gfx.getFontMetrics();
    }

    public FontMetrics getFontMetrics(Font f)
    {
        return gfx.getFontMetrics(f);
    }

    public boolean hitClip(int x, int y, int width, int height)
    {
        return gfx.hitClip(x, y, width, height);
    }

    public void setClip(int x, int y, int width, int height)
    {
        gfx.setClip(x, y, width, height);
    }

    public void setClip(Shape clip)
    {
        gfx.setClip(clip);
    }

    public void setColor(Color c)
    {
        gfx.setColor(c);
    }

    public void setFont(Font font)
    {
        gfx.setFont(font);
    }

    public void setPaintMode()
    {
        gfx.setPaintMode();
    }

    public void setXORMode(Color c1)
    {
        gfx.setXORMode(c1);
    }

    public void translate(int x, int y)
    {
        gfx.translate(x, y);
    }

    /*
     * DVBGraphics methods
     */
    public int[] getAvailableCompositeRules()
    {
        int[] rules = {
            DVBAlphaComposite.CLEAR,
            DVBAlphaComposite.SRC,
            DVBAlphaComposite.SRC_OVER };

        return rules;
    }

    public DVBAlphaComposite getDVBComposite()
    {
        Composite comp = gfx.getComposite();
        if (!(comp instanceof AlphaComposite))
            return null;
        return DVBAlphaComposite.getInstance(
                        ((AlphaComposite)comp).getRule(),
                        ((AlphaComposite)comp).getAlpha());
    }

    public void setDVBComposite(DVBAlphaComposite comp)
            throws UnsupportedDrawingOperationException
    {
        if ((comp.getRule() < DVBAlphaComposite.CLEAR) ||
            (comp.getRule() > DVBAlphaComposite.SRC_OVER))
            throw new UnsupportedDrawingOperationException("Unsupported composition rule");

        gfx.setComposite(AlphaComposite.getInstance(comp.getRule(), comp.getAlpha()));
    }

    /*
     * Graphics2D methods
     */
    public Composite getComposite()
    {
        return gfx.getComposite();
    }

    public Stroke getStroke()
    {
        return gfx.getStroke();
    }

    public GraphicsConfiguration getDeviceConfiguration()
    {
        return gfx.getDeviceConfiguration();
    }

    public void setComposite(Composite comp)
    {
        gfx.setComposite(comp);
    }

    public void setStroke(Stroke s)
    {
        gfx.setStroke(s);
    }
}
