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

public class HStaticIcon extends HVisible implements HNoInputPreferred {
    public HStaticIcon() {
        super(getDefaultLook());
    }

    public HStaticIcon(Image imageNormal, int x, int y, int width, int height) {
        super(getDefaultLook(), x, y, width, height);
        setGraphicContent(imageNormal, NORMAL_STATE);
    }

    public HStaticIcon(Image imageNormal) {
        super(getDefaultLook());
        setGraphicContent(imageNormal, NORMAL_STATE);
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        if ((hlook != null) && !(hlook instanceof HGraphicLook)) {
            throw new HInvalidLookException();
        }
        super.setLook(hlook);
    }

    public static void setDefaultLook(HGraphicLook hlook) {
        BDJXletContext.setXletDefaultLook(PROPERTY_LOOK,hlook);
    }

    public static HGraphicLook getDefaultLook() {
        return (HGraphicLook) BDJXletContext.getXletDefaultLook(PROPERTY_LOOK,DEFAULT_LOOK);
    }

    private static final String PROPERTY_LOOK = "HStaticIcon";
    static final Class DEFAULT_LOOK = HGraphicLook.class;
    private static final long serialVersionUID = 2015589998794748072L;
}
