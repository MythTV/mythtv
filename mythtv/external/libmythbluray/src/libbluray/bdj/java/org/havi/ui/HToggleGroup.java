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

import java.util.Enumeration;
import java.util.Vector;

public class HToggleGroup {

    public HToggleGroup() {
    }

    public HToggleButton getCurrent() {
        return current;
    }

    public void setCurrent(HToggleButton selection) {
        // Treat null as unselection
        if (selection == null) {
            HToggleButton tmp = getCurrent();
            if (tmp != null && getForcedSelection() && buttons.size() > 0) {
                // Enforce forced selection (reselect button!)
                tmp.setSwitchableState(true);
            } else {
                // General behavior
                current = null;
                if (tmp != null) tmp.setSwitchableState(false);
            }
        }
        // Set current only if part of this group
        // And isn't the current selection (stops infinite loop)
        else if (buttons.contains(selection) && getCurrent() != selection) {
            current = selection;
            selection.setSwitchableState(true);
            unswitch(selection); // Enforce single selection
        }
    }

    public void setForcedSelection(boolean forceSelection) {
        this.forceSelection = forceSelection;

        // Enforce new setting
        if (forceSelection && getCurrent() == null && buttons.size() > 0) {
            forceSelect();
        }
    }

    public boolean getForcedSelection() {
        return forceSelection;
    }

    public void setEnabled(boolean enable) {
        enabled = enable;
        for (Enumeration e = buttons.elements(); e.hasMoreElements();) {
            setEnabled((HToggleButton) e.nextElement(), enable);
        }
    }

    public boolean isEnabled() {
        return enabled;
    }

    protected void add(HToggleButton button) {
        // Only add if not already added
        if (!buttons.contains(button)) {
            buttons.addElement(button);
            setEnabled(button, isEnabled()); // Enforce enabled state

            // Enforce forced selection (if first addition)
            if (getForcedSelection() && buttons.size() == 1 && getCurrent() != button) {
                button.setSwitchableState(true);
                current = button;
                // Assume that if size()>=1 that it's already enforced!
            }
            // If currently selected, unselect all others!
            else if (button.getSwitchableState()) {
                current = button;
                if (buttons.size() > 1) unswitch(button); // Enforce single
                                                          // selection
            }
        }
    }

    protected void remove(HToggleButton button) {
        if (!buttons.removeElement(button)) {
            throw new IllegalArgumentException("Not a member of this HToggleGroup");
        } else {
            if (button == getCurrent()) {
                current = null;
                if (getForcedSelection() && buttons.size() > 0) {
                    current = null;
                    forceSelect();
                }
            }
        }
    }


    private void unswitch(HToggleButton button) {
        for (Enumeration e = buttons.elements(); e.hasMoreElements();) {
            HToggleButton b = (HToggleButton) e.nextElement();
            if (b != button) b.setSwitchableState(false);
        }
    }

    private void forceSelect() {
        // assert(getCurrent() == null);
        if (buttons.size() > 0) {
            HToggleButton b = (HToggleButton) buttons.elementAt(0);
            b.setSwitchableState(true);
            current = b;
        }
    }

    private void setEnabled(HToggleButton tb, boolean enable) {
        if (false) { // If HAVi 1.1
            tb.setEnabled(enable);
        } else {
            // HAVI 1.01beta
            int state = tb.getInteractionState();
            tb.setInteractionState(enable ? (state & ~HState.DISABLED_STATE_BIT) : (state | HState.DISABLED_STATE_BIT));
        }
    }

    /** Whether the buttons in this group are enabled or now. */
    private boolean enabled = true;

    /** Controls whether a selection must always be made or not. */
    private boolean forceSelection = false;

    /** The currently selected {@link HToggleButton}. */
    private HToggleButton current = null;

    /** The buttons currently added to this group. */
    private Vector buttons = new Vector();
}
