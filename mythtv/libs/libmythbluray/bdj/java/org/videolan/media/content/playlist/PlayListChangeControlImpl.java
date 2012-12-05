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

import javax.media.ClockStartedError;
import javax.tv.locator.InvalidLocatorException;
import javax.tv.locator.Locator;

import org.bluray.media.InvalidPlayListException;
import org.bluray.media.PlayListChangeControl;
import org.bluray.net.BDLocator;
import org.bluray.ti.PlayList;

public class PlayListChangeControlImpl implements PlayListChangeControl {
    protected PlayListChangeControlImpl(Handler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public void selectPlayList(PlayList pl) throws InvalidPlayListException, ClockStartedError {
        try {
                player.selectPlayList((BDLocator)pl.getLocator());
        } catch (InvalidLocatorException e) {
            throw new InvalidPlayListException();
        }
    }

    public void selectPlayList(BDLocator locator)
            throws InvalidPlayListException, InvalidLocatorException, ClockStartedError {
        player.selectPlayList(locator);
    }

    public BDLocator getCurrentPlayList() {
        Locator[] locators = player.getServiceContentLocators();
        if ((locators == null) || (locators.length <= 0))
            return null;
        return (BDLocator)locators[0];
    }

    private Handler player;
}
