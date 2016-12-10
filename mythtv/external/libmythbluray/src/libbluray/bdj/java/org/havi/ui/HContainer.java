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
import org.dvb.ui.TestOpacity;

//https://www.jinahya.com/mvn/site/com.googlecode.jinahya/ocap-api/1.3.1/apidocs/org/havi/ui/HContainer.html

public class HContainer extends Container implements HMatteLayer,
        HComponentOrdering, TestOpacity {
    public HContainer() {
        this(0,0,0,0);
    }

    public HContainer(int x, int y, int width, int height) {
        setBounds(x,y,width,height);
    }

    public void setMatte(HMatte m) throws HMatteException {
        org.videolan.Logger.unimplemented("HContainer", "setMatte");
        throw new HMatteException("Matte is not supported");
    }

    public HMatte getMatte() {
        return null;
    }

    public boolean isDoubleBuffered() {
        return false;   // can this be true ?
    }

    public boolean isOpaque() {
        return false;   // can this be true ?
    }

    private int getOffset(java.awt.Component c) throws ArrayIndexOutOfBoundsException {
        Component cs[] = getComponents();
        for (int i = 0; i < cs.length; ++i)
            if (cs[i] == c) return i;

        throw new ArrayIndexOutOfBoundsException("Component not contained within");
    }

    private void checkLineage(java.awt.Component c) {
        if (c.getParent() != this) throw new ArrayIndexOutOfBoundsException("Component not contained within");
    }

    public Component addBefore(Component component, Component behind) {
        // check to see if behind is an element of this container
        try {
            getOffset(behind);
        } catch (Exception e) {
            return null;
        }

        if (component == behind) return component;
        synchronized (getTreeLock()) {
            try {
                // Explicitly remove component if in this container.
                // Should have no effect if not in this container.
                // This must be done so that problems don't occur
                // when componentIndex < frontIndex.
                remove(component);

                int offset = getOffset(behind);

                return add(component, offset);
            } catch (Exception e) {
                return null;
            }
        }
    }

    public Component addAfter(Component component, Component front) {
        // check to see if front is an element of this container
        try {
            getOffset(front);
        } catch (Exception e) {
            return null;
        }

        if (component == front) return component;
        synchronized (getTreeLock()) {
            try {
                // Explicitly remove component if in this container.
                // Should have no effect if not in this container.
                // This must be done so that problems don't occur
                // when componentIndex < frontIndex.
                remove(component);

                int offset = getOffset(front);

                return add(component, offset + 1);
            } catch (Exception e) {
                return null;
            }
        }
    }

    public boolean popToFront(Component component) {
        synchronized (getTreeLock()) {
            try {
                // Ensure it is there
                checkLineage(component);

                // explicitly remove component
                // (even if reparenting is implicit)
                remove(component);
                add(component, 0);
                return true;
            } catch (Exception e) {
                return false;
            }
        }
    }

    public boolean pushToBack(Component component) {
        synchronized (getTreeLock()) {
            try {
                // Ensure it is there
                checkLineage(component);

                // explicitly remove component
                // (even if reparenting is implicit)
                remove(component);
                add(component, -1);
                return true;
            } catch (Exception e) {
                return false;
            }
        }
    }

    public boolean pop(Component component) {
        synchronized (getTreeLock()) {
            try {
                int offset = getOffset(component);

                if (offset > 0) {
                    // explicitly remove component
                    // (even if reparenting is implicit)
                    remove(component);
                    add(component, offset - 1);
                    return true;
                }
            } catch (Exception e) {
            }
            return false;
        }
    }

    public boolean push(Component component) {
        synchronized (getTreeLock()) {
            try {
                int offset = getOffset(component);
                int count = getComponentCount();

                if (offset == (count - 1)) {
                    return true;
                }

                if (offset < (count - 1)) {
                    // explicitly remove component
                    // (even if reparenting is implicit)
                    remove(component);
                    add(component, offset + 1);
                    return true;
                }
            } catch (Exception e) {
            }

            return false;
        }
    }

    public boolean popInFrontOf(Component move, Component behind) {
        synchronized (getTreeLock()) {
            try {
                if (move != behind) {
                    // Ensure they are present
                    checkLineage(move);
                    checkLineage(behind);

                    // explicitly remove component
                    // (even if reparenting is implicit)
                    remove(move);
                    // Re-add
                    addBefore(move, behind);
                }
                return true;
            } catch (Exception e) {
                return false;
            }
        }
    }

    public boolean pushBehind(Component move, Component front) {
        synchronized (getTreeLock()) {
            try {
                if (move != front) {
                    // Ensure they are present
                    checkLineage(move);
                    checkLineage(front);

                    // explicitly remove component
                    // (even if reparenting is implicit)
                    remove(move);
                    // re-add in proper location
                    addAfter(move, front);
                }
                return true;
            } catch (Exception e) {
                return false;
            }
        }
    }

    public void group() {
        grouped = true;
    }

    public void ungroup() {
        grouped = false;
    }

    public boolean isGrouped() {
       return grouped;
    }

    private boolean grouped = false;

    private static final long serialVersionUID = 263606166411114032L;
}
