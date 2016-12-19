package org.bluray.ti;

import javax.tv.locator.Locator;
import javax.tv.service.SIException;
import javax.tv.service.SIRequest;
import javax.tv.service.SIRequestor;
import javax.tv.service.ServiceType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.TitleInfo;
import org.videolan.bdjo.Bdjo;

public class TitleImpl implements Title {
    public TitleImpl(int titleNum) throws SIException {
        this.titleNum = titleNum;
        this.ti = Libbluray.getTitleInfo(titleNum);
        if (ti == null)
            throw new SIException("Title " + titleNum + " does not exist in disc index");
        if (ti.isBdj()) {
            bdjo = Libbluray.getBdjo(ti.getBdjoName());
            if (bdjo == null)
                throw new SIException("title " + titleNum + ": Failed loading " + ti.getBdjoName() + ".bdjo");
        }
    }

    public PlayList[] getPlayLists() {
        if (bdjo == null)
            return new PlayList[0];
        String[] playlistNames = bdjo.getAccessiblePlaylists().getPlayLists();
        PlayList[] playlists = new PlayList[playlistNames.length];
        for (int i = 0; i < playlistNames.length; i++)
            playlists[i] = new PlayListImpl(playlistNames[i], this);

        return playlists;
    }

    public boolean hasAutoPlayList() {
        if (bdjo == null)
            return false;
        return bdjo.getAccessiblePlaylists().isAutostartFirst();
    }

    public Locator getLocator() {
        String url = "bd://" + Integer.toString(titleNum, 16);
        try {
            return new BDLocator(url);
        } catch (InvalidLocatorException ex) {
            return null;
        }
    }

    public String getName() {
        if (titleNum == 0)
            return "Top Menu";
        if (titleNum == 65535)
            return "First Playback";
        if (titleNum == 65534)
            return "Suspended Title";
        return "Title " + titleNum;
    }

    public ServiceType getServiceType() {
        switch (ti.getPlaybackType()) {
        case TitleInfo.HDMV_PLAYBACK_TYPE_MOVIE:
            return TitleType.HDMV_MOVIE;
        case TitleInfo.HDMV_PLAYBACK_TYPE_INTERACTIVE:
            return TitleType.HDMV_INTERACTIVE;
        case TitleInfo.BDJ_PLAYBACK_TYPE_MOVIE:
            return TitleType.BDJ_MOVIE;
        case TitleInfo.BDJ_PLAYBACK_TYPE_INTERACTIVE:
            return TitleType.BDJ_INTERACTIVE;
        }
        return TitleType.UNKNOWN;
    }

    public boolean equals(Object obj) {
        if (!(obj instanceof TitleImpl)) {
            return false;
        }
        TitleImpl other = (TitleImpl)obj;
        int otherNum = other.getTitleNum();
        return otherNum == titleNum;
    }

    public boolean hasMultipleInstances() {
        return false;
    }

    public SIRequest retrieveDetails(SIRequestor requestor) {
        //TODO
        org.videolan.Logger.unimplemented(TitleImpl.class.getName(), "retrieveDetails");
        return null;
    }

    public int getTitleNum() {
        return titleNum;
    }

    public TitleInfo getTitleInfo() {
        return ti;
    }

    private int titleNum;
    private TitleInfo ti;
    private Bdjo bdjo = null;
}
