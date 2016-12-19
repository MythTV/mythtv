/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

import java.awt.AWTEvent;
import java.awt.Component;
import org.dvb.ui.TestOpacity;

import java.awt.BDToolkit;

public abstract class HComponent extends Component implements HMatteLayer, TestOpacity {

    public HComponent() {
        this(0, 0, 0, 0);
        BDToolkit.addComponent(this);
    }

    public HComponent(int x, int y, int width, int height) {
        setBounds(x, y, width, height);
    }

    public void setMatte(HMatte m) throws HMatteException {
        matte = m;
    }

    public HMatte getMatte() {
        return matte;
    }

    public boolean isDoubleBuffered() {
        return false;
    }

    public boolean isOpaque() {
        return false;
    }

    public void setEnabled(boolean b) {
        if (b != super.isEnabled()) {
            super.setEnabled(b);
            super.setFocusable(b);
            super.setVisible(b);
        }
    }

    public boolean isEnabled() {
        return super.isEnabled();
    }

    protected void processEvent(AWTEvent event) {
        org.videolan.Logger.unimplemented(HComponent.class.getName(), "processEvent");

        super.processEvent(event);
    }

    private HMatte matte = null;

    private static final long serialVersionUID = -4115249517434074428L;
}
