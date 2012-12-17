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

package javax.tv.media;

import java.awt.Dimension;
import javax.media.Control;

public interface AWTVideoSizeControl extends Control {
    public AWTVideoSize getSize();

    public AWTVideoSize getDefaultSize();

    public Dimension getSourceVideoSize();

    public boolean setSize(AWTVideoSize size);

    public AWTVideoSize checkSize(AWTVideoSize size);
}
