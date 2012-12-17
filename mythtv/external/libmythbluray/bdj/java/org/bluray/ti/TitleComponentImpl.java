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
    protected TitleComponentImpl(int stn, StreamInfo stream, StreamType type, boolean primary, int playlistId, int playitemId, Title service)
    {
        this.stn = stn;
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
            str += ".A1:" + stn;
        else if (type.equals(StreamType.VIDEO) && primary)
            str += ".V1:" + stn;
        else if (type.equals(StreamType.AUDIO) && !primary)
            str += ".A2:" + stn;
        else if (type.equals(StreamType.VIDEO) && !primary)
            str += ".V2:" + stn;
        else if (type.equals(StreamType.SUBTITLES) && primary)
            str += ".P:" + stn;
        else
            return null;

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
        throw new Error("Not implemented"); // TODO implement
    }

    public CodingType getCodingType() {
        return stream.getCodingType();
    }

    public int getStreamNumber() {
        return stn;
    }

    public int getSubPathId() {
        return stream.getSubPathId();
    }

    int stn;
    StreamInfo stream;
    StreamType type;
    boolean primary;
    int playlistId;
    int playitemId;
    Title service;
}
