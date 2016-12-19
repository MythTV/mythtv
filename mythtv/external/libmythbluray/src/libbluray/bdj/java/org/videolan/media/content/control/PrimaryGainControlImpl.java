/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2015  Petri Hintukainen
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

package org.videolan.media.content.control;

import java.awt.Component;
import org.bluray.media.PrimaryGainControl;

import org.videolan.media.content.BDHandler;

public class PrimaryGainControlImpl extends GainControlImpl implements PrimaryGainControl {
    public PrimaryGainControlImpl(BDHandler player) {
        this.player = player;
    }

    protected void setGain(boolean mute, float level) {
        player.setGain(BDHandler.GAIN_PRIMARY, mute, level);
        super.valueChanged();
    }

    private BDHandler player;
}
