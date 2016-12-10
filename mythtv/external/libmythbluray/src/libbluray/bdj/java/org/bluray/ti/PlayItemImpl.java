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
import javax.tv.service.navigation.ServiceComponent;
import javax.tv.service.navigation.StreamType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class PlayItemImpl implements PlayItem {
    protected PlayItemImpl(int playlistId, int playitemId, TIClip clip, Title service) {
        this.playlistId = playlistId;
        this.playitemId = playitemId;
        this.clip = clip;
        this.service = service;
    }

    public int getPlayItemId() {
        return playitemId;
    }

    public int getPlayListId() {
        return playlistId;
    }

    public Locator getLocator() {
        int title = Libbluray.getCurrentTitle();
        try {
            return new BDLocator("bd://" + Integer.toHexString(title) + ".PLAYLIST:" + playlistId + ".ITEM:" + playitemId);
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

    public ServiceComponent[] getComponents() {
        StreamInfo[] video = clip.getVideoStreams();
        StreamInfo[] audio = clip.getVideoStreams();
        StreamInfo[] pg = clip.getVideoStreams();
        StreamInfo[] ig = clip.getVideoStreams();
        StreamInfo[] secVideo = clip.getVideoStreams();
        StreamInfo[] secAudio = clip.getVideoStreams();

        int count = video.length + audio.length + pg.length +
            ig.length + secVideo.length + secAudio.length;

        ServiceComponent[] components = new ServiceComponent[count];

        int i = 0;
        for (int j = 0; j < video.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, video[j], StreamType.VIDEO, true, playlistId, playitemId, service);
        for (int j = 0; j < audio.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, audio[j], StreamType.AUDIO, true, playlistId, playitemId, service);
        for (int j = 0; j < pg.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, pg[j], StreamType.SUBTITLES, true, playlistId, playitemId, service);
        for (int j = 0; j < ig.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, ig[j], StreamType.DATA, true, playlistId, playitemId, service);
        for (int j = 0; j < secVideo.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, secVideo[j], StreamType.VIDEO, false, playlistId, playitemId, service);
        for (int j = 0; j < secAudio.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, secAudio[j], StreamType.AUDIO, false, playlistId, playitemId, service);

        return components;
    }

    private int playlistId;
    private int playitemId;
    private TIClip clip;
    private Title service;
}
