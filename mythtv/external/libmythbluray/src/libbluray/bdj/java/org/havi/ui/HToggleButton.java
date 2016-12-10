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

import java.awt.Image;

import org.videolan.BDJXletContext;

public class HToggleButton extends HGraphicButton implements HSwitchable {
    public HToggleButton() {
        super();
        iniz();
    }

    public HToggleButton(Image image, int x, int y, int width, int height) {
        super(image, x, y, width, height);
        iniz();
    }

    public HToggleButton(Image image) {
        super(image);
        iniz();
    }

    public HToggleButton(Image image, int x, int y, int width, int height,
            boolean state) {
        this(image, x, y, width, height);
        setSwitchableState(state);
    }

    public HToggleButton(Image imageNormal, Image imageFocused,
            Image imageActioned, Image imageNormalActioned, int x, int y,
            int width, int height, boolean state) {
        super(imageNormal, imageFocused, imageActioned, x, y, width, height);
        setGraphicContent(imageNormalActioned, ACTIONED_STATE);
        setSwitchableState(state);
        iniz();
    }

    public HToggleButton(Image imageNormal, Image imageFocused,
            Image imageActioned, Image imageNormalActioned, boolean state) {
        super(imageNormal, imageFocused, imageActioned);
        setGraphicContent(imageNormalActioned, ACTIONED_STATE);
        setSwitchableState(state);
        iniz();
    }

    public HToggleButton(Image image, int x, int y, int width, int height,
            boolean state, HToggleGroup group) {
        this(image, x, y, width, height, state);
        setToggleGroup(group);
    }

    public HToggleButton(Image image, boolean state, HToggleGroup group) {
        this(image);
        setSwitchableState(state);
        setToggleGroup(group);
    }

    public HToggleButton(Image imageNormal, Image imageFocused,
            Image imageActioned, Image imageNormalActioned, int x, int y,
            int width, int height, boolean state, HToggleGroup group) {
        this(imageNormal, imageFocused, imageActioned, imageNormalActioned, x, y, width, height, state);
        setToggleGroup(group);
    }

    public HToggleButton(Image imageNormal, Image imageFocused,
            Image imageActioned, Image imageNormalActioned, boolean state,
            HToggleGroup group) {
        this(imageNormal, imageFocused, imageActioned, imageNormalActioned, state);
        setToggleGroup(group);
    }

    private void iniz() {
        try {
            setLook(getDefaultLook());
        } catch (HInvalidLookException ignored) {
        }
    }

    public void setToggleGroup(HToggleGroup group) {
        HToggleGroup oldGroup = toggleGroup;

        // Remove ourselves if already a member of a group
        if (oldGroup != null) {
            // If it is the same, don't do anything.
            if (oldGroup == group)
                return;

            // Remove ourselves
            oldGroup.remove(this);
        }

        // Assign the new toggle group
        toggleGroup = group;
        if (group != null) {
            group.add(this);
        }
    }

    public HToggleGroup getToggleGroup() {
        return toggleGroup;
    }

    public void removeToggleGroup() {
        setToggleGroup(null);
    }

    public static void setDefaultLook(HGraphicLook hlook) {
        BDJXletContext.setXletDefaultLook(PROPERTY_LOOK, hlook);
    }

    public static HGraphicLook getDefaultLook() {
        return (HGraphicLook) BDJXletContext.getXletDefaultLook(PROPERTY_LOOK, DEFAULT_LOOK);
    }

    public boolean getSwitchableState() {
        return (getInteractionState() & ACTIONED_STATE_BIT) != 0;
    }

    public void setSwitchableState(boolean state) {
        int old = getInteractionState();
        setInteractionState(state ? (old | ACTIONED_STATE_BIT) : (old & ~ACTIONED_STATE_BIT));
    }

    public void setUnsetActionSound(HSound sound) {
        unsetActionSound = sound;
    }

    public HSound getUnsetActionSound() {
        return unsetActionSound;
    }

    private static final String PROPERTY_LOOK = "HToggleButton";
    private HToggleGroup toggleGroup = null;
    private HSound unsetActionSound = null;

    private static final long serialVersionUID = 2602166176018744707L;
}
