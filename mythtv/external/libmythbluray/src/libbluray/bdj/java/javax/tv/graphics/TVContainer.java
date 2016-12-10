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
package javax.tv.graphics;

import java.awt.Container;
import javax.tv.xlet.XletContext;
import org.havi.ui.HSceneFactory;

import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class TVContainer {
    public static Container getRootContainer(XletContext context)
    {
        if (context == null) {
            Logger.getLogger(TVContainer.class.getName()).error("null context");
            throw new NullPointerException();
        }

        if (!(context instanceof BDJXletContext) || (BDJXletContext)context != BDJXletContext.getCurrentContext()) {
            Logger.getLogger(TVContainer.class.getName()).error("wrong context");
        }

        /* GEM: return instance of org.havi.ui.HScene or NULL */
        HSceneFactory sf = HSceneFactory.getInstance();
        if (sf != null) {
            return sf.getDefaultHScene();
        }
        return null;
    }
}
