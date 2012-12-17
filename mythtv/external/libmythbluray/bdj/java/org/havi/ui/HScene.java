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
import java.util.Map;

import org.havi.ui.event.HEventGroup;
import org.videolan.BDJXletContext;
import org.videolan.GUIManager;

public class HScene extends Container implements HComponentOrdering {
    protected HScene() {
        context = BDJXletContext.getCurrentContext();
    }

    public BDJXletContext getXletContext() {
        return context;
    }

    public void paint(Graphics g) {
        if (backgroundMode == BACKGROUND_FILL) {
            g.setColor(getBackground());
            g.fillRect(super.getX(), super.getY(), super.getWidth(), super
                    .getHeight());
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
        return true;// not implemented
    }

    public boolean isOpaque() {
        return false;// not implemented
    }

    public Component addBefore(Component component, Component behind) {
        int index = super.getComponentZOrder(behind);

        // the behind component must already be in the Container
        if (index == -1)
            return null;

        return super.add(component, index);
    }

    public Component addAfter(Component component, Component front) {
        int index = super.getComponentZOrder(front);

        // the front component must already be in the Container
        if (index == -1)
            return null;

        return super.add(component, index + 1);
    }

    public boolean popToFront(Component component) {
        if (super.getComponentZOrder(component) == -1)
            return false;

        super.setComponentZOrder(component, 0);

        return true;
    }

    public boolean pushToBack(Component component) {
        if (super.getComponentZOrder(component) == -1)
            return false;

        super.setComponentZOrder(component, super.getComponentCount());

        return true;
    }

    public boolean pop(Component component) {
        int index = super.getComponentZOrder(component);
        if (index == -1)
            return false;
        if (index == 0)
            return true; // already at the front

        super.setComponentZOrder(component, index - 1);
        return true;
    }

    public boolean push(Component component) {
        int index = super.getComponentZOrder(component);
        if (index == -1)
            return false;
        if (index == super.getComponentCount())
            return true; // already at the back

        super.setComponentZOrder(component, index + 1);
        return true;
    }

    public boolean popInFrontOf(Component move, Component behind) {
        int index = super.getComponentZOrder(behind);

        // the behind component must already be in the Container
        if (index == -1 || super.getComponentZOrder(move) == -1)
            return false;

        super.setComponentZOrder(move, index);
        return true;
    }

    public boolean pushBehind(Component move, Component front) {
        int index = super.getComponentZOrder(front);

        // the behind component must already be in the Container
        if (index == -1 || super.getComponentZOrder(move) == -1)
            return false;

        super.setComponentZOrder(move, index + 1);
        return true;
    }

    public void addWindowListener(WindowListener listener) {
        synchronized (windowListener) {
            windowListener = HEventMulticaster.add(windowListener, listener);
        }
    }

    public void removeWindowListener(WindowListener listener) {
        synchronized (windowListener) {
            windowListener = HEventMulticaster.remove(windowListener, listener);
        }
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

        for (Component comp : super.getComponents()) {
            if (comp.hasFocus())
                return comp;
        }

        return null;
    }

    public void show() {
        setVisible(true);
    }

    public void dispose() {
        // not implemented
    }

    public boolean addShortcut(int keyCode, HActionable act) {
        // make sure component is in HScene
        boolean hasComp = false;
        for (Component comp : getComponents()) {
            if (comp == act)
                hasComp = true;
        }

        if (!hasComp)
            return false;

        shortcuts.put(keyCode, act);

        return true;
    }

    public void removeShortcut(int keyCode) {
        shortcuts.remove(keyCode);
    }

    public HActionable getShortcutComponent(int keyCode) {
        return shortcuts.get(keyCode);
    }

    public void enableShortcuts(boolean enable) {
        this.shortcutsEnabled = enable;
    }

    public boolean isEnableShortcuts() {
        return shortcutsEnabled;
    }

    public int getShortcutKeycode(HActionable comp) {
        for (Integer key : shortcuts.keySet()) {
            HActionable action = shortcuts.get(key);

            if (action == comp)
                return key;
        }

        return KeyEvent.VK_UNDEFINED;
    }

    public int[] getAllShortcutKeycodes() {
        Integer[] src = (Integer[]) shortcuts.keySet().toArray();
        int[] dest = new int[src.length];

        for (int i = 0; i < src.length; i++)
            dest[i] = src[i];

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
        this.visible = visible;
    }

    public boolean isVisible() {
        return visible;
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

    private boolean visible = true;
    private boolean active = false;
    private int backgroundMode = NO_BACKGROUND_FILL;
    private Image image = null;
    private int imageMode = IMAGE_NONE;
    private WindowListener windowListener = null;
    private HEventGroup eventGroup = null;
    private Map<Integer, HActionable> shortcuts = Collections
            .synchronizedMap(new HashMap<Integer, HActionable>());
    private boolean shortcutsEnabled = true;
    private BDJXletContext context;

    private static final long serialVersionUID = 422730746877212409L;
}
