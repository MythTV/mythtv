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
import javax.tv.service.Service;
import javax.tv.service.ServiceInformationType;
import javax.tv.service.navigation.StreamType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.StreamInfo;

public class TitleComponentImpl implements TitleComponent {
    protected TitleComponentImpl(int stream_number, StreamInfo stream, StreamType type, boolean primary,
                                 int playlistId, int playitemId, Title service) {
        this.stream_number = stream_number;
        this.stream = stream;
        this.type = type;
        this.primary = primary;
        this.playlistId = playlistId;
        this.playitemId = playitemId;
        this.service = service;
    }

    public String getName() {
        BDLocator l = (BDLocator) getLocator();
        if (l == null)
            return null;
        return l.toString();
    }

    public String getAssociatedLanguage() {
        return stream.getLang();
    }

    public StreamType getStreamType() {
        return type;
    }

    public Service getService() {
        return service;
    }

    public Locator getLocator() {
        String str;

        str = "bd://" + ((TitleImpl)service).getTitleNum() +
            ".PLAYLIST:" + playlistId +
            ".ITEM:" + playitemId;

        if (type.equals(StreamType.AUDIO) && primary)
            str += ".A1:" + stream_number;
        else if (type.equals(StreamType.VIDEO) && primary)
            str += ".V1:" + stream_number;
        else if (type.equals(StreamType.AUDIO) && !primary)
            str += ".A2:" + stream_number;
        else if (type.equals(StreamType.VIDEO) && !primary)
            str += ".V2:" + stream_number;
        else if (type.equals(StreamType.SUBTITLES) && primary)
            str += ".P:" + stream_number;
        else {
            System.err.println("Unknown StreamType " + type);
            return null;
        }

        try {
            return new BDLocator(str);
        } catch (InvalidLocatorException e) {
            return null;
        }
    }

    public ServiceInformationType getServiceInformationType() {
        return TitleInformationType.BD_ROM;
    }

    public Date getUpdateTime() {
        org.videolan.Logger.unimplemented(TitleComponentImpl.class.getName(), "getUpdateTime");
        return null;
    }

    public CodingType getCodingType() {
        return stream.getCodingType();
    }

    public int getStreamNumber() {
        return stream_number;
    }

    public int getSubPathId() {
        return stream.getSubPathId();
    }

    private int stream_number;
    private StreamInfo stream;
    private StreamType type;
    private boolean primary;
    private int playlistId;
    private int playitemId;
    private Title service;
}
