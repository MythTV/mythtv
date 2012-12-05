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

import org.havi.ui.event.HKeyEvent;
import org.havi.ui.event.HTextEvent;

public interface HKeyboardInputPreferred {
    public boolean getEditMode();

    public void setEditMode(boolean edit);

    public int getType();

    public char[] getValidInput();

    public void processHTextEvent(HTextEvent event);

    public void processHKeyEvent(HKeyEvent event);

    public static final int INPUT_NUMERIC = 1;
    public static final int INPUT_ALPHA = 2;
    public static final int INPUT_ANY = 4;
    public static final int INPUT_CUSTOMIZED = 8;
}
