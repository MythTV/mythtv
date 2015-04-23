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

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Insets;

public interface HLook extends java.lang.Cloneable {
    public void showLook(Graphics g, HVisible visible, int state);

    public void widgetChanged(HVisible visible, HChangeData[] changes);

    public Dimension getMinimumSize(HVisible hvisible);

    public Dimension getPreferredSize(HVisible hvisible);

    public Dimension getMaximumSize(HVisible hvisible);

    public boolean isOpaque(HVisible visible);

    public Insets getInsets(HVisible hvisible);
}
