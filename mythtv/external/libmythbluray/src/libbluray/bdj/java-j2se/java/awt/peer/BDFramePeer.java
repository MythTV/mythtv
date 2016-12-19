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
import java.awt.image.BufferedImage;

import java.security.AccessController;
import java.security.PrivilegedAction;

import org.videolan.Logger;

public class BDFramePeer extends BDComponentPeer implements FramePeer
{
    public BDFramePeer(Frame frame, BDRootWindow rootWindow) {
        super(frame.getToolkit(), frame);
        this.rootWindow = rootWindow;
    }

    public Rectangle getBoundsPrivate() {
        return null;
    }

    public int getState() {
        return Frame.NORMAL;
    }

    public void setBoundsPrivate(int a, int b, int c, int d) {
    }

    public void setMaximizedBounds(Rectangle bounds) {
    }

    public void setMenuBar(MenuBar mb) {
    }

    public void setResizable(boolean resizeable) {
    }

    public void setState(int state) {
    }

    public void setTitle(String title) {
    }

    /* Java 8 */
    public void emulateActivation(boolean doActivate) {
    }

    //
    // ContainerPeer
    //

    public void beginLayout() {
    }

    public void beginValidate() {
    }

    public void endLayout() {
    }

    public void endValidate() {
    }

    public Insets getInsets() {
        return insets;
    }

    /* java 1.6 only */
    public Insets insets() {
        return getInsets();
    }

    /* java 1.6 only */
    public boolean isPaintPending() {
        return false;
    }

    /* java 1.6 only */
    public boolean isRestackSupported() {
        return false;
    }

    /* java 1.6 only */
    public void restack() {
    }

    //
    // WindowPeer
    //

    public void repositionSecurityWarning() {
    }

    /* Removed in java 1.6 update 45 */
    public void setAlwaysOnTop(boolean b) {
    }

    /* java 1.6 update 45. Also in java 7 / 8. */
    public void updateAlwaysOnTopState() {
    }

    public void setModalBlocked(Dialog d,boolean b) {
    }

    public void setOpacity(float f) {
    }

    public void setOpaque(boolean b) {
    }

    public void toBack() {
    }

    public void toFront() {
    }

    public void updateFocusableWindowState() {
    }

    public void updateIconImages() {
    }

    public void updateMinimumSize() {
    }

    public void updateWindow(BufferedImage b) {
        logger.unimplemented("updateWindow");
    }

    /* java 1.7 ? */
    public void updateWindow() {
        logger.unimplemented("updateWindow");
    }

    /* java 1.6 only */
    public boolean requestWindowFocus()  {
        return true;
    }

    //
    // ComponentPeer
    //

    //public Rectangle getBounds() {
    //    return rootWindow.getBounds();
    //}

    public Graphics getGraphics() {
        return new BDWindowGraphics(rootWindow);
    }

    public Image createImage(int width, int height) {
        return ((BDToolkit)BDToolkit.getDefaultToolkit()).createImage((Component)null, width, height);
    }

    public boolean requestFocus(Component c, boolean a, boolean b, long l, sun.awt.CausedFocusEvent.Cause d) {
        if (c == null) {
            return true;
        }
        final FocusEvent focusEvent = new FocusEvent(c, FocusEvent.FOCUS_GAINED);
        AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    Toolkit.getDefaultToolkit().getSystemEventQueue().postEvent(focusEvent);
                    return null;
                }
            });
        return true;
    }

    public void setVisible(boolean b) {
        //Toolkit.getDefaultToolkit().getSystemEventQueue().postEvent(new WindowEvent((Frame)component, WindowEvent.WINDOW_ACTIVATED));
        if (b == true) {
            Graphics g = getGraphics();
            component.paint(g);
            g.dispose();
        }
    }

    public void dispose() {
        super.dispose();
        rootWindow = null;
    }

    private BDRootWindow rootWindow;
    private Insets insets = new Insets(0, 0, 0, 0);

    private static final Logger logger = Logger.getLogger(BDFramePeer.class.getName());
}
