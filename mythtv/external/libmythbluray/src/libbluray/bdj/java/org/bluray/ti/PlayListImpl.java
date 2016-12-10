/*
 * This file is part of libbluray
 * Copyright (C) 2010-2015 VideoLAN
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

package org.bluray.ti;

import java.util.Date;

import javax.tv.locator.Locator;
import javax.tv.service.ServiceInformationType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.TIClip;
import org.videolan.PlaylistInfo;

public class PlayListImpl implements PlayList {
    protected PlayListImpl(String filename, Title service) {
        this.filename = filename;
        this.id = Integer.parseInt(filename);
        this.playlist = Libbluray.getPlaylistInfo(id);
        this.service = service;
    }

    public String getFileName() {
        return filename;
    }

    public int getId() {
        return id;
    }

    public PlayItem[] getPlayItems() {
        TIClip[] clips = playlist.getClips();
        PlayItem[] items = new PlayItem[clips.length];

        for (int i = 0; i < clips.length; i++) {
            items[i] = new PlayItemImpl(id, i + 1, clips[i], service);
        }

        return items;
    }

    public Locator getLocator() {
        int title = Libbluray.getCurrentTitle();

        try {
            return new BDLocator(null, title, id);
        } catch (InvalidLocatorException e) {
            return null;
        }
    }

    public ServiceInformationType getServiceInformationType() {
        return TitleInformationType.BD_ROM;
    }

    public Date getUpdateTime() {
        return null;
    }

    private String filename;
    private PlaylistInfo playlist;
    private int id;
    private Title service;
}
