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

package org.havi.ui;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Rectangle;
import java.awt.Image;
import java.awt.event.KeyEvent;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.havi.ui.event.HEventGroup;
import org.videolan.BDJXletContext;
import org.videolan.GUIManager;
import org.videolan.Logger;
import java.awt.BDToolkit;


public class HScene extends Container implements HComponentOrdering {
    protected HScene() {
        context = BDJXletContext.getCurrentContext();
        if (context == null) {
            logger.error("HScene() created from privileged context: " + Logger.dumpStack());
        }
        BDToolkit.addComponent(this);
    }

    public BDJXletContext getXletContext() {
        return context;
    }

    public void paint(Graphics g) {
        if (backgroundMode == BACKGROUND_FILL) {
            g.setColor(getBackground());
            g.fillRect(super.getX(), super.getY(), super.getWidth(), super.getHeight());
        }

        if (image != null) {
            switch (imageMode) {
            case IMAGE_CENTER:
                g.drawImage(image, (super.getWidth() - image.getWidth(null)) / 2,
                        (super.getHeight() - image.getHeight(null)) / 2, null);
                break;
            case IMAGE_STRETCH:
                g.drawImage(image, super.getX(), super.getY(), super.getWidth(), super.getHeight(), null);
                break;
            case IMAGE_TILE:
                for (int x = super.getX(); x < super.getWidth(); x += image.getWidth(null))
                    for (int y = super.getY(); y < super.getHeight(); y += image.getHeight(null))
                        g.drawImage(image, x, y, null);
                break;
            }
        }

        super.paint(g);
    }

    public boolean isDoubleBuffered() {
        return false;
    }

    public boolean isOpaque() {
        return false;
    }

    public int getComponentZOrder(Component component) {
        for (int i = 0; i < getComponentCount(); i++)
            if (getComponent(i) == component)
                return i;
        return -1;
    }

    public /*boolean*/void setComponentZOrder(Component component, int index) {
        if (index < 0)
            return;//return false;
        int i = getComponentZOrder(component);
        if (i < 0)
            return;//return false;
        if (i == index)
            return;//return true;
        remove(component);
        if (i > index)
            add(component, index);
        else
            add(component, index - 1);
        //return true;
    }

    public Component addBefore(Component component, Component behind) {
        int index = getComponentZOrder(behind);
        if (index < 0)
            return null;

        return super.add(component, index);
    }

    public Component addAfter(Component component, Component front) {
        int index = getComponentZOrder(front);
        if (index < 0)
            return null;

        return super.add(component, index + 1);
    }

    public boolean popToFront(Component component) {
        if (getComponentZOrder(component) < 0)
            return false;
        /*return */setComponentZOrder(component, 0);
        return true;
    }

    public boolean pushToBack(Component component) {
        if (getComponentZOrder(component) < 0)
            return false;
        /*return */setComponentZOrder(component, getComponentCount());
        return true;
    }

    public boolean pop(Component component) {
        int index = getComponentZOrder(component);
        if (index < 0)
            return false;
        if (index == 0)
            return true; // already at the front

        /*return */setComponentZOrder(component, index - 1);
        return true;
    }

    public boolean push(Component component) {
        int index = getComponentZOrder(component);
        if (index < 0)
            return false;

        if (index == getComponentCount())
            return true; // already at the back

        /*return */setComponentZOrder(component, index + 1);
        return true;
    }

    public boolean popInFrontOf(Component move, Component behind) {
        int index = getComponentZOrder(behind);

        // the behind component must already be in the Container
        if (index < 0 || getComponentZOrder(move) < 0)
            return false;

        /*return */setComponentZOrder(move, getComponentZOrder(behind));
        return true;
    }

    public boolean pushBehind(Component move, Component front) {
        int index = getComponentZOrder(front);
        if (index < 0 || getComponentZOrder(move) < 0)
            return false;

        /*return */setComponentZOrder(move, index + 1);
        return true;
    }

    public void addWindowListener(WindowListener listener) {
        windowListener = HEventMulticaster.add(windowListener, listener);
    }

    public void removeWindowListener(WindowListener listener) {
        windowListener = HEventMulticaster.remove(windowListener, listener);
    }

    protected void processWindowEvent(WindowEvent event) {
        if (windowListener != null) {
            switch (event.getID()) {
            case WindowEvent.WINDOW_OPENED:
                windowListener.windowOpened(event);
                break;
            case WindowEvent.WINDOW_CLOSING:
                windowListener.windowClosing(event);
                break;
            case WindowEvent.WINDOW_CLOSED:
                windowListener.windowClosed(event);
                break;
            case WindowEvent.WINDOW_ICONIFIED:
                windowListener.windowIconified(event);
                break;
            case WindowEvent.WINDOW_DEICONIFIED:
                windowListener.windowDeiconified(event);
                break;
            case WindowEvent.WINDOW_ACTIVATED:
                windowListener.windowActivated(event);
                break;
            case WindowEvent.WINDOW_DEACTIVATED:
                windowListener.windowDeactivated(event);
                break;
            }
        }

        switch (event.getID()) {
        case WindowEvent.WINDOW_ACTIVATED:
            active = true;
            break;
        case WindowEvent.WINDOW_DEACTIVATED:
            active = false;
            break;
        }
    }

    public Component getFocusOwner() {
        if (!active)
            return null;

        Component[] comps = getComponents();
        for (int i = 0; i < comps.length; i++) {
            if (comps[i].hasFocus())
                return comps[i];
        }

        return null;
    }

    public synchronized void dispose() {
        HSceneFactory sf = HSceneFactory.getInstance();
        if (sf != null) {
            sf.dispose(this);
        }
    }

    protected void disposeImpl()
    {
        // called by HSceneFactory
        try {
            removeAll();

            Graphics g = GUIManager.getInstance().getGraphics();
            if (g != null) {
                Rectangle r = getBounds();
                g.clearRect(r.x, r.y, r.width, r.height);
                g.dispose();
            }
            if (image != null) {
                image.flush();
            }
            if (shortcuts != null) {
                shortcuts.clear();
            }
        } finally {
            image = null;
            eventGroup = null;
            shortcuts = null;
            windowListener = null;
            context = null;
        }
    }

    public boolean addShortcut(int keyCode, HActionable act) {
        // make sure component is in HScene
        boolean hasComp = false;

        Component[] comps = getComponents();
        for (int i = 0; i < comps.length; i++) {
            if (comps[i] == act) {
                hasComp = true;
                break;
            }
        }

        if (!hasComp)
            return false;

        shortcuts.put(new Integer(keyCode), act);

        return true;
    }

    public void removeShortcut(int keyCode) {
        shortcuts.remove(new Integer(keyCode));
    }

    public HActionable getShortcutComponent(int keyCode) {
        return (HActionable)shortcuts.get(new Integer(keyCode));
    }

    public void enableShortcuts(boolean enable) {
        this.shortcutsEnabled = enable;
    }

    public boolean isEnableShortcuts() {
        return shortcutsEnabled;
    }

    public int getShortcutKeycode(HActionable comp) {
        Iterator iterator = shortcuts.keySet().iterator();
        Integer key;
        while ((key = (Integer)iterator.next()) != null) {
            HActionable action = (HActionable)shortcuts.get(key);
            if (action == comp)
                return key.intValue();
        }

        return KeyEvent.VK_UNDEFINED;
    }

    public int[] getAllShortcutKeycodes() {
        Object[] src = shortcuts.keySet().toArray();
        int[] dest = new int[src.length];

        for (int i = 0; i < src.length; i++)
            dest[i] = ((Integer)src[i]).intValue();

        return dest;
    }

    public HScreenRectangle getPixelCoordinatesHScreenRectangle(Rectangle rect) {
        Dimension size = GUIManager.getInstance().getSize();
        return new HScreenRectangle(rect.x / (float) size.width, rect.y
                / (float) size.height, rect.width / (float) size.width,
                rect.height / (float) size.height);
    }

    public HSceneTemplate getSceneTemplate() {
        HSceneTemplate template = new HSceneTemplate();

        HGraphicsConfiguration config = HScreen.getDefaultHScreen()
                .getDefaultHGraphicsDevice().getCurrentConfiguration();
        Rectangle sceneRect = super.getBounds();
        HScreenRectangle screenRect = getPixelCoordinatesHScreenRectangle(sceneRect);

        template.setPreference(HSceneTemplate.GRAPHICS_CONFIGURATION, config,
                HSceneTemplate.PREFERRED);
        template.setPreference(HSceneTemplate.SCENE_PIXEL_DIMENSION, sceneRect
                .getSize(), HSceneTemplate.PREFERRED);
        template.setPreference(HSceneTemplate.SCENE_PIXEL_LOCATION, sceneRect
                .getLocation(), HSceneTemplate.PREFERRED);
        template.setPreference(HSceneTemplate.SCENE_SCREEN_DIMENSION,
                new HScreenDimension(screenRect.width, screenRect.height),
                HSceneTemplate.PREFERRED);
        template.setPreference(HSceneTemplate.SCENE_SCREEN_LOCATION,
                new HScreenPoint(screenRect.x, screenRect.y),
                HSceneTemplate.PREFERRED);

        return template;
    }

    public void setActive(boolean focus) {
        if (active == true && focus == false)
            dispatchEvent(new WindowEvent(GUIManager.getInstance(),
                    WindowEvent.WINDOW_DEACTIVATED));

        active = focus;
    }

    public void setKeyEvents(HEventGroup eventGroup) {
        this.eventGroup = eventGroup;
    }

    public HEventGroup getKeyEvents() {
        return eventGroup;
    }

    public void setVisible(boolean visible) {
        if (visible == isVisible())
            return;
        super.setVisible(visible);

        /*
         * This doesn't work
         *  - visible state of other HScenes is never restored
         *     => no focus, key events are lost
         *  - Scenes are not wiped from GUI

        if (visible) {
            for (int i = 0; i < GUIManager.getInstance().getComponentCount(); i++) {
                    Component c = GUIManager.getInstance().getComponent(i);
                    if (c != this)
                        c.setVisible(false);
            }
        }
        */

        if (visible)
            GUIManager.getInstance().setVisible(visible);
    }

    public int getBackgroundMode() {
        return backgroundMode;
    }

    public void setBackgroundMode(int mode) {
        this.backgroundMode = mode;
    }

    public void setBackgroundImage(Image image) {
        this.image = image;
    }

    public Image getBackgroundImage() {
        return image;
    }

    public boolean setRenderMode(int mode) {
        this.imageMode = mode;
        return true;
    }

    public int getRenderMode() {
        return imageMode;
    }

    public static final int IMAGE_NONE = 0;
    public static final int IMAGE_STRETCH = 1;
    public static final int IMAGE_CENTER = 2;
    public static final int IMAGE_TILE = 3;

    public static final int NO_BACKGROUND_FILL = 0;
    public static final int BACKGROUND_FILL = 1;

    private boolean active = false;
    private int backgroundMode = NO_BACKGROUND_FILL;
    private Image image = null;
    private int imageMode = IMAGE_NONE;
    private WindowListener windowListener = null;
    private HEventGroup eventGroup = null;
    private Map shortcuts = Collections.synchronizedMap(new HashMap());
    private boolean shortcutsEnabled = true;
    private BDJXletContext context;

    private static final Logger logger = Logger.getLogger(HScene.class.getName());

    private static final long serialVersionUID = 422730746877212409L;
}
