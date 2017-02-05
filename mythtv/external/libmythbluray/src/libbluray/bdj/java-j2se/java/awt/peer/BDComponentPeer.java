/*
 * This file is part of libbluray
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package java.awt.peer;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

import org.videolan.Logger;

public abstract class BDComponentPeer implements ComponentPeer
{
    public BDComponentPeer(Toolkit toolkit, Component component) {
        this.toolkit = toolkit;
        this.component = component;
        setBounds (component.getX(), component.getY(), component.getWidth(), component.getHeight(), SET_BOUNDS);
    }

    public void applyShape(sun.java2d.pipe.Region r) {
    }

    public boolean canDetermineObscurity() {
        return false;
    }

    public int checkImage(Image img, int w, int h, ImageObserver o) {
        return ((BDToolkit)toolkit).checkImage(img, w, h, o);
    }

    public void coalescePaintEvent(PaintEvent e) {
    }

    public void createBuffers(int x, BufferCapabilities bufferCapabilities) {
        logger.unimplemented("createBuffers");
    }

    public Image createImage(ImageProducer producer) {
        logger.unimplemented("createImage");
        return null;
    }

    public Image createImage(int width, int height) {
        Component parent = component.getParent();
        if (parent != null) {
            return parent.createImage(width, height);
        }
        logger.error("createImage(): no parent !");
        throw new Error();
    }

    public VolatileImage createVolatileImage(int width, int height) {
        logger.unimplemented("createVolatileImage");
        return null;
    }

    public void destroyBuffers() {
    }

    /* java 6 only */
    public void disable() {
        setEnabled(false);
    }

    public void dispose() {
        component = null;
        toolkit = null;
    }

    /* java 6 only */
    public void enable() {
        setEnabled(true);
    }

    public void flip(int a, int b, int c, int d, java.awt.BufferCapabilities.FlipContents e) {
    }

    /* java 6 only */
    public Rectangle getBounds() {
        return new Rectangle(location.x, location.y, size.width, size.height);
        //rootWindow.getBounds();
    }

    public Image getBackBuffer() {
        logger.unimplemented("getBackBuffer");
        throw new Error();
    }

    public ColorModel getColorModel() {
        return toolkit.getColorModel();
    }

    public FontMetrics getFontMetrics(Font font) {
        logger.unimplemented("getFontMetrics");
        return null;
    }

    public Graphics getGraphics() {
        Component parent = component.getParent();
        if (parent != null) {
            Graphics g = parent.getGraphics();
            if (g != null) {
                return g.create(location.x, location.y, size.width, size.height);
            }
        }
        logger.error("getGraphics(): no parent !");
        throw new Error();
    }

    public GraphicsConfiguration getGraphicsConfiguration() {
        logger.unimplemented("getGraphicsConfiguration");
        return null;
    }

    public Point getLocationOnScreen() {
        Point screen = new Point(location);
        Component parent = component.getParent();
        if (parent != null) {
            Point parentScreen = parent.getLocationOnScreen();
            screen.translate(parentScreen.x, parentScreen.y);
        }
        return screen;
    }

    public Dimension getMinimumSize() {
        return size;
    }

    public Dimension getPreferredSize() {
        return size;
    }

    public Toolkit getToolkit() {
        return toolkit;
    }

    public void handleEvent(AWTEvent e) {
        int id = e.getID();

        if (e instanceof PaintEvent) {
            Graphics g = null;
            Rectangle r = ((PaintEvent)e).getUpdateRect();
            try {
                g = component.getGraphics();
                if (g == null)
                    return;
                g.clipRect(r.x, r.y, r.width, r.height);
                if (id == PaintEvent.PAINT)
                    component.paint(g);
                else
                    component.update(g);
                toolkit.sync();
            } finally {
                if (g != null)
                    g.dispose();
            }
        }
    }

    public boolean handlesWheelScrolling() {
        return false;
    }

    /* java 6 only */
    public void hide() {
        setVisible(false);
    }

    public boolean isFocusable() {
        return true;
    }

    public boolean isObscured() {
        return false;
    }

    public boolean isReparentSupported() {
        return false;
    }

    public void layout() {
    }

    /* java 1.6 only */
    public Dimension minimumSize() {
        return getMinimumSize();
    }

    public void paint(Graphics g) {
        component.paint(g);
    }

    /* java 1.6 only */
    public Dimension preferredSize() {
        return getPreferredSize();
    }

    public boolean prepareImage(Image img, int w, int h, ImageObserver o) {
        return ((BDToolkit)toolkit).prepareImage(img, w, h, o);
    }

    public void print(Graphics g) {
    }

    /* java 1.6 only */
    public void repaint(long tm, int x, int y, int width, int height) {
        logger.unimplemented("repaint");
    }

    public void reparent(ContainerPeer p) {
    }

    /* java 1.6 only */
    public void reshape(int x, int y, int width, int height) {
        setBounds(x, y, width, height, SET_BOUNDS);
    }

    public boolean requestFocus(Component lightweightChild, boolean temporary, boolean focusedWindowChangeAllowed, long time) {
        return true;
    }


    public void setBackground(Color c) {
    }

    public void setBounds(int x, int y, int width, int height, int op) {
        location.x = x;
        location.y = y;
        size.width = width;
        size.height = height;
    }

    public void setEnabled(boolean b) {
        logger.unimplemented("setEnabled");
    }

    public void setFont(Font f) {
    }

    public void setForeground(Color c) {
    }

    public void setVisible(boolean b) {
    }

    /* java 7 */
    public void setZOrder(ComponentPeer peer) {
    }

    /* java 6 only */
    public void show() {
        setVisible(true);
    }

    public void updateCursorImmediately() {
    }

    /* java 7 */
    public boolean updateGraphicsData(GraphicsConfiguration gc) {
        return false;
    }


    protected Component component;
    protected Toolkit toolkit;
    protected Point location = new Point();
    protected Dimension size = new Dimension();

    private static final Logger logger = Logger.getLogger(BDComponentPeer.class.getName());
}
