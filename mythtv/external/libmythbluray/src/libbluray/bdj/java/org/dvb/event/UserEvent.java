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

package org.dvb.event;

import java.awt.event.KeyEvent;

public class UserEvent extends java.util.EventObject {
    public UserEvent(Object source, int family, int type, int code,
            int modifiers, long when)
    {
        super(source);
        
        this.family = family;
        this.type = type;
        this.code = code;
        this.modifiers = modifiers;
        this.when = when;
    }

    public UserEvent(Object source, int family, char keyChar, long when)
    {
        super(source);
       
        this.family = family;
        this.type = KeyEvent.KEY_TYPED;
        this.code = keyChar;
        this.modifiers = 0;
        this.when = when;
    }

    public int getFamily()
    {
        return family;
    }

    public int getType()
    {
        return type;
    }

    public int getCode()
    {
        return code;
    }

    public char getKeyChar()
    {
        return (char) code;
    }

    public int getModifiers()
    {
        return modifiers;
    }

    public boolean isShiftDown()
    {
        return (KeyEvent.SHIFT_MASK & modifiers) != 0;
    }

    public boolean isControlDown()
    {
        return (KeyEvent.CTRL_MASK & modifiers) != 0;
    }

    public boolean isMetaDown()
    {
        return (KeyEvent.META_MASK & modifiers) != 0;
    }

    public boolean isAltDown()
    {
        return (KeyEvent.ALT_MASK & modifiers) != 0;
    }

    public long getWhen()
    {
        return when;
    }
    
    public static final int UEF_KEY_EVENT = 1;

    private int family;
    private int type;
    private int code;
    private int modifiers;
    private long when;
    private static final long serialVersionUID = -4734616177850745290L; 
}
