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

package org.bluray.ui.event;

import java.awt.Component;

public abstract class HRcEvent extends org.havi.ui.event.HRcEvent {
    public HRcEvent(Component source, int id, long when, int modifiers,
            int keyCode, char keyChar)
    {
        super(source, id, when, modifiers, keyCode, keyChar);
    }

    public static final int VK_PG_TEXTST_ENABLE_DISABLE = 465;
    public static final int VK_POPUP_MENU = 461;
    public static final int VK_SECONDARY_AUDIO_ENABLE_DISABLE = 463;
    public static final int VK_SECONDARY_VIDEO_ENABLE_DISABLE = 464;
    public static final int VK_STILL_OFF = 462;

    private static final long serialVersionUID = 3477897216005481682L;
}
