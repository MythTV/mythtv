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

package org.videolan.media.content.playlist;

import java.awt.Component;

import org.bluray.media.AudioMetadataControl;

// TODO: Figure out what the hell this is for
public class AudioMetadataControlImpl implements AudioMetadataControl {
    public Component getControlComponent() {
        return null;
    }

    public boolean isMetaDataProcessingOn() {
        throw new Error("Not implemented"); // TODO implement
    }

    public void setMetaDataProcessingOn(boolean value) {
        throw new Error("Not implemented"); // TODO implement
    }

}
