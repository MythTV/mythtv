/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package java.awt;

import sun.awt.NullGraphics;

public class BDToolkit extends BDToolkitBase {

    public BDToolkit () {
    }

    Graphics getGraphics(Window window) {
        if (!(window instanceof BDRootWindow)) {
            return new NullGraphics(window);
        }
        return new BDWindowGraphics((BDRootWindow)window);
    }

    public void sync() {
        Window window = ((BDGraphicsDevice)getLocalGraphicsEnvironment().getDefaultScreenDevice()).getWindow();
        if (window instanceof BDRootWindow)
            ((BDRootWindow)window).sync();
    }
}
