/*
 * This file is part of libbluray
 * Copyright (C) 2012-2014  libbluray
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

import java.awt.font.*;
import java.awt.image.renderable.RenderableImage;
import java.awt.image.RenderedImage;
import java.awt.geom.AffineTransform;
import java.text.AttributedCharacterIterator;

import org.videolan.Logger;

class BDGraphics extends BDGraphicsBase {
    private Paint paint;

    BDGraphics(BDGraphics g) {
        super(g);
    }

    BDGraphics(BDRootWindow window) {
        super(window);
    }

    public Color getBackground() {
        return background;
    }
    public void setBackground(Color c) {
        if (c != null) {
            background = c;
        }
    }

    BDGraphics(BDImage image) {
        super(image);
    }

    public java.awt.font.FontRenderContext getFontRenderContext()
    {
        logger.unimplemented("getFontRenderContext");
        return null;
    }
    public void setPaint(Paint p) {
        logger.unimplemented("setPaint");
        paint = p;
    }
    public Paint getPaint() {
        return paint;
    }
    public void transform(java.awt.geom.AffineTransform t) {
        logger.unimplemented("transform");
    }
    public void setTransform(java.awt.geom.AffineTransform t) {
        logger.unimplemented("setTransform");
    }
    public java.awt.geom.AffineTransform getTransform() {
        logger.unimplemented("getTransform");
        throw new Error("Not implemented");
    }
    public void shear(double a, double b) {
        logger.unimplemented("shear");
    }
    public void scale(double a, double b) {
        logger.unimplemented("scale");
    }
    public void rotate(double a) {
        logger.unimplemented("rotate");
    }
    public void rotate(double a, double b, double c) {
        logger.unimplemented("rotate");
    }
    public void translate(double a, double b) {
        logger.unimplemented("translate");
    }
    public boolean hit(Rectangle rect, Shape s, boolean onStroke)  {
        logger.unimplemented("hit");
        return true;
    }
    public void fill(Shape s) {
        logger.unimplemented("fill");
    }
    public void draw(java.awt.Shape s) {
        logger.unimplemented("draw");
    }
    public  void drawGlyphVector(GlyphVector g, float x, float y)  {
        logger.unimplemented("drawGlyphVector");
    }
    public void setRenderingHints(java.util.Map hints) {
        logger.unimplemented("setRenderingHints");
    }
    public void setRenderingHint(RenderingHints.Key hintKey, Object hintValue) {
        logger.unimplemented("setRenderingHint");
    }
    public void addRenderingHints(java.util.Map hints) {
        logger.unimplemented("addRenderingHints");
    }
    public Object getRenderingHint(RenderingHints.Key hintKey) {
        logger.unimplemented("getRenderingHint");
        return null;
    }
    public RenderingHints getRenderingHints() {
        logger.unimplemented("getRenderingHints");
        return null;
    }


    public void clip(Shape s) {
        setClip(s);
    }


    public void drawString(String string, float x, float y) {
        drawString(string, (int)x, (int)y);
    }

    public void drawRenderableImage(RenderableImage img, AffineTransform xform)  {
        logger.unimplemented("drawRenaerableImage");
    }

    public void drawRenderedImage(RenderedImage img, AffineTransform xform) {
        logger.unimplemented("drawRenaeredImage");
    }

    public void drawString(AttributedCharacterIterator arg0, int arg1, int arg2) {
        logger.unimplemented("drawString");
    }

    public void drawString(AttributedCharacterIterator iterator, float x, float y) {
        logger.unimplemented("drawString");
    }

    public void drawImage(java.awt.image.BufferedImage i,java.awt.image.BufferedImageOp o, int x, int y) {
        logger.unimplemented("drawImage");
    }

    public boolean drawImage(java.awt.Image i, java.awt.geom.AffineTransform t, java.awt.image.ImageObserver o) {
        logger.unimplemented("drawImage");
        return true;
    }

    private static final Logger logger = Logger.getLogger(BDGraphics.class.getName());
}
