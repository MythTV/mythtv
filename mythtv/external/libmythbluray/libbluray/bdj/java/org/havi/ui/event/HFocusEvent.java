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

package org.havi.ui.event;

public class HFocusEvent extends java.awt.event.FocusEvent {
    public static final int HFOCUS_FIRST = HTextEvent.TEXT_LAST + 1;
    public static final int FOCUS_TRANSFER = HFOCUS_FIRST;
    public static final int HFOCUS_LAST = FOCUS_TRANSFER;
    public static final int NO_TRANSFER_ID = -1;

    public HFocusEvent(java.awt.Component source, int id)
    {
        super(source, id, false);

        this.transfer = NO_TRANSFER_ID;
    }

    public HFocusEvent(java.awt.Component source, int id, int transfer)
    {
        super(source, id, false);

        this.transfer = transfer;
    }

    public boolean isTemporary()
    {
        return false;
    }

    public int getTransferId()
    {
        return transfer;
    }

    private int transfer;
    private static final long serialVersionUID = -159334433682866327L;
}
