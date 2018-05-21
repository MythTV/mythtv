/*
 * This file is part of libbluray
 * Copyright (C) 2010      William Hahne
 * Copyright (C) 2012-2014 Petri Hintukainen <phintuka@users.sourceforge.net>
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

import java.io.IOException;

import javax.media.ClockStartedError;
import javax.media.ConnectionErrorEvent;
import javax.media.Control;
import javax.media.ControllerErrorEvent;
import javax.media.IncompatibleSourceException;
import javax.media.Time;
import javax.media.protocol.DataSource;
import javax.tv.locator.Locator;
import javax.tv.locator.InvalidLocatorException;
import javax.tv.service.selection.ServiceContextFactory;

import org.bluray.media.InvalidPlayListException;
import org.bluray.net.BDLocator;
import org.bluray.system.RegisterAccess;
import org.bluray.ti.selection.TitleContextImpl;

import org.videolan.BDJAction;
import org.videolan.Libbluray;
import org.videolan.Logger;
import org.videolan.PlaylistInfo;
import org.videolan.TIClip;
import org.videolan.media.content.BDHandler;

import org.videolan.media.content.control.MediaTimePositionControlImpl;
import org.videolan.media.content.control.OverallGainControlImpl;
import org.videolan.media.content.control.PanningControlImpl;
import org.videolan.media.content.control.PrimaryGainControlImpl;
import org.videolan.media.content.control.SecondaryGainControlImpl;

public class Handler extends BDHandler {
    public Handler() {
        controls = new Control[18];
        controls[0] = new AngleControlImpl(this);
        controls[1] = new AudioMetadataControlImpl();
        controls[2] = new BackgroundVideoPresentationControlImpl(this);
        controls[3] = new DVBMediaSelectControlImpl(this);
        controls[4] = new MediaTimeEventControlImpl();
        controls[5] = new MediaTimePositionControlImpl(this);
        controls[6] = new OverallGainControlImpl(this);
        controls[7] = new PanningControlImpl(this);
        controls[8] = new PiPControlImpl(this);
        controls[9] = new PlaybackControlImpl(this);
        controls[10] = new PlayListChangeControlImpl(this);
        controls[11] = new PrimaryAudioControlImpl(this);
        controls[12] = new PrimaryGainControlImpl(this);
        controls[13] = new SecondaryAudioControlImpl(this);
        controls[14] = new SecondaryGainControlImpl(this);
        controls[15] = new SubtitlingControlImpl(this);
        controls[16] = new UOMaskTableControlImpl(this);
        controls[17] = new VideoFormatControlImpl(this);
    }

    public void setSource(DataSource source) throws IOException, IncompatibleSourceException {

        /* validate source */
        BDLocator newLocator;
        try {
            newLocator = new BDLocator(source.getLocator().toExternalForm());
        } catch (org.davic.net.InvalidLocatorException e) {
            logger.error("incompatible source: " + source);
            throw new IncompatibleSourceException();
        } catch (Exception e) {
            logger.error("unexpected expection: " + e);
            throw new IncompatibleSourceException();
        }

        if (!newLocator.isPlayListItem()) {
            logger.error("not playlist: " + newLocator);
            throw new IncompatibleSourceException();
        }

        /* get playlist info */
        PlaylistInfo newPi = Libbluray.getPlaylistInfo(newLocator.getPlayListId());
        if (newPi == null) {
            logger.error("getPlaylistInfo failed for " + newLocator);
            throw new IOException();
        }

        /* commit changes and prefetch */
        synchronized (this) {

            sourceLocator = newLocator;
            currentLocator = null;
            pi = newPi;

            baseMediaTime = 0;
            if (state == Prefetched)
                doPrefetch();
        }
    }

    public Locator[] getServiceContentLocators() {
        if (sourceLocator == null)
            return new Locator[0];
        Locator[] locators = new Locator[1];
        if (currentLocator != null && getState() >= Prefetched)
            locators[0] = currentLocator;
        else
            locators[0] = sourceLocator;
        return locators;
    }

    public Time getDuration() {
        if (pi != null) {
            long duration = pi.getDuration();
            return new Time(duration * TO_SECONDS);
        }
        return DURATION_UNKNOWN;
    }

    protected ControllerErrorEvent doPrefetch() {
        synchronized (this) {
            try {
                int stream;
                stream = sourceLocator.getPrimaryAudioStreamNumber();
                if (stream > 0)
                    Libbluray.writePSR(Libbluray.PSR_PRIMARY_AUDIO_ID, stream);
                stream = sourceLocator.getPGTextStreamNumber();
                if (stream > 0) {
                    Libbluray.writePSR(Libbluray.PSR_PG_STREAM, stream, 0x00000fff);
                }
                stream = sourceLocator.getSecondaryVideoStreamNumber();
                if (stream > 0) {
                    Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, stream << 8, 0x0000ff00);
                }
                stream = sourceLocator.getSecondaryAudioStreamNumber();
                if (stream > 0) {
                    Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, stream, 0x000000ff);
                }

                int plId = sourceLocator.getPlayListId();
                long time = -1;
                int piId = -1, mark = -1;
                if (baseMediaTime != 0) {
                    time = (long)(baseMediaTime * FROM_NAROSECONDS);
                } /*else*/ if (sourceLocator.getMarkId() > 0) {
                    mark = sourceLocator.getMarkId();
                } /*else*/ if (sourceLocator.getPlayItemId() > 0) {
                    piId = sourceLocator.getPlayItemId();
                }

                if (!Libbluray.selectPlaylist(plId, piId, mark, time)) {
                    return new ConnectionErrorEvent(this);
                }

                updateTime(new Time(Libbluray.tellTime() * TO_SECONDS));

                currentLocator = new BDLocator(sourceLocator.toExternalForm());
            } catch (Exception e) {
                return new ConnectionErrorEvent(this);
            }
            return super.doPrefetch();
        }
    }

    protected ControllerErrorEvent doStart(Time at) {
        synchronized (this) {
            if (at != null) {
                try {
                    Libbluray.seekTime((long)(at.getSeconds() * FROM_SECONDS));
                } catch (Exception e) {
                    return new ConnectionErrorEvent(this);
                }
            }

            try {
                Libbluray.selectRate(rate, true);
            } catch (Exception e) {
                return new ConnectionErrorEvent(this);
            }

            at = new Time(Libbluray.tellTime() * TO_SECONDS);
            return super.doStart(at);
        }
    }

    protected ControllerErrorEvent doStop(boolean eof) {
        Libbluray.selectRate(0.0f, false);
        if (!eof) {
            Libbluray.stopPlaylist();
        }
        return super.doStop(eof);
    }

    protected void doSeekTime(Time at) {
        synchronized (this) {
            if ((state == Prefetched) || (state == Started)) {
                try {
                    Libbluray.seekTime((long)(at.getSeconds() * FROM_SECONDS));
                } catch (Exception e) {
                    return;
                }
                at = new Time(Libbluray.tellTime() * TO_SECONDS);
            }
            super.doSeekTime(at);
        }
    }

    protected void doSetRate(Float factor) {
        synchronized (this) {
            if (state == Started) {
                try {
                    Libbluray.selectRate(factor.floatValue());
                } catch (Exception e) {
                    return;
                }
                if (state == Started) {
                    baseMediaTime = getMediaNanoseconds();
                    baseTime = getTimeBase().getNanoseconds();
                }
            }
            super.doSetRate(factor);
        }
    }

    /* notification from app */

    private void postMediaSelectSucceeded(BDLocator locator) {
        ((DVBMediaSelectControlImpl)controls[3]).postMediaSelectSucceededEvent(new Locator[] { locator });
    }

    protected void doRateChanged(float rate) {
        synchronized (this) {
            if (state == Started) {
                baseMediaTime = getMediaNanoseconds();
                baseTime = getTimeBase().getNanoseconds();
            }
            super.doRateChanged(rate);
        }
    }

    protected void doChapterReached(int param) {
        ((PlaybackControlImpl)controls[9]).onChapterReach(param);
    }

    protected void doMarkReached(int param) {
        ((PlaybackControlImpl)controls[9]).onMarkReach(param);

        if (currentLocator != null)
            currentLocator.setMarkId(param);
    }

    protected void doPlaylistStarted(int param) {
    }

    protected void doPlayItemReached(int param) {
        ((PlaybackControlImpl)controls[9]).onPlayItemReach(param);
        ((UOMaskTableControlImpl)controls[16]).onPlayItemReach(param);


        if (currentLocator != null) {
            currentLocator.setPlayItemId(param);
            postMediaSelectSucceeded(currentLocator);
        }

        try {
            ((TitleContextImpl)ServiceContextFactory.getInstance().getServiceContext(null)).presentationChanged();
        } catch (Exception e) {
            System.err.println("" + e + "\n" + Logger.dumpStack(e));
        }

        if (pi != null) {
            TIClip[] clips = pi.getClips();
            if (clips != null && param >= 0 && param < clips.length) {
                ((SubtitlingControlImpl)controls[15]).onSubtitleAvailable(clips[param].getPgStreamCount() > 0);
            }
        }
    }

    protected void doUOMasked(int position) {
        ((UOMaskTableControlImpl)controls[16]).onUOMasked(position);
    }

    protected void doAngleChanged(int param) {
        ((AngleControlImpl)controls[0]).onAngleChange(param);
    }

    protected void doSubtitleChanged(int param) {
        ((SubtitlingControlImpl)controls[15]).onSubtitleChange(param);

        if (currentLocator != null) {
            currentLocator.setPGTextStreamNumber(param & 0xfff);
            postMediaSelectSucceeded(currentLocator);
        }
    }

    protected void doAudioStreamChanged(int param) {
        if (currentLocator != null) {
            currentLocator.setPrimaryAudioStreamNumber(param);
            postMediaSelectSucceeded(currentLocator);
        }
    }

    private void doSecondaryVideoChanged(int stream, boolean enable) {
        if (currentLocator != null) {
            currentLocator.setSecondaryVideoStreamNumber(stream);
            postMediaSelectSucceeded(currentLocator);
        }

        ((PiPControlImpl)controls[8]).onPiPStatusChange(enable);
    }

    private void doSecondaryAudioChanged(int stream, boolean enable) {
        if (currentLocator != null) {
            currentLocator.setSecondaryAudioStreamNumber(stream);
            postMediaSelectSucceeded(currentLocator);
        }
    }

    protected void doSecondaryStreamChanged(int param) {
        doSecondaryVideoChanged((param & 0xff00) >> 8, (param & 0x80000000) != 0);
        doSecondaryAudioChanged(param & 0xff, (param & 0x40000000) != 0);
    }

    protected void doEndOfMediaReached(int playlist) {
        synchronized (this) {
            if (currentLocator == null) {
                logger.error("endOfMedia(" + playlist + ") ignored: no current locator");
                return;
            }
            if (currentLocator.getPlayListId() != playlist) {
                logger.error("endOfMedia ignored: playlist does not match (" + playlist + " != " + currentLocator.getPlayListId());
                return;
            }
        }

        super.doEndOfMediaReached(playlist);
    }

    protected void doSeekNotify(long tick) {
        super.doSeekNotify(Libbluray.tellTime());
    }

    /* used by DVBMediaSelectControlImpl */
    protected BDLocator getCurrentLocator() {
        if (currentLocator != null)
            return currentLocator;
        return sourceLocator;
    }

    protected PlaylistInfo getPlaylistInfo() {
        return pi;
    }

    protected TIClip getCurrentClipInfo() {
        synchronized (this) {
            int state = getState();
            if ((state != Prefetched) && (state != Started))
                return null;

            int playitem = RegisterAccess.getInstance().getPSR(RegisterAccess.PSR_PLAYITEM_ID);
            TIClip[] clips = pi.getClips();
            if (playitem >= clips.length)
                return null;
            return clips[playitem];
        }
    }

    protected void selectPlayList(BDLocator locator)
            throws InvalidPlayListException, InvalidLocatorException, ClockStartedError {

        if (locator == null) {
            logger.error("null locator");
            throw new NullPointerException();
        }
        if (!locator.isPlayListItem()) {
            logger.error("not playlist: " + locator);
            throw new InvalidLocatorException(locator);
        }

        PlaylistInfo newPi = Libbluray.getPlaylistInfo(locator.getPlayListId());
        if (newPi == null) {
            logger.error("invalid playlist");
            throw new InvalidPlayListException();
        }

        synchronized (this) {
            if (getState() == Started) {
                logger.error("throw ClockStartedError");
                throw new ClockStartedError();
            }

            this.pi = newPi;
            this.sourceLocator = locator;
            this.currentLocator = null;

            baseMediaTime = 0;
            if (state == Prefetched)
                doPrefetch();
        }
    }

    protected void seekMark(int mark) throws IllegalArgumentException {
        if ((pi == null) || (mark < 0) || (mark >= pi.getMarkCount()))
            throw new IllegalArgumentException();
        PlaylistPlayerAction action = new PlaylistPlayerAction(
                this, PlaylistPlayerAction.ACTION_SEEK_MARK, mark);
        commandQueue.put(action);
        action.waitEnd();
    }

    protected void seekPlayItem(int item) throws IllegalArgumentException {
        if ((pi == null) || (item < 0) || (item >= pi.getClipCount()))
            throw new IllegalArgumentException();
        PlaylistPlayerAction action = new PlaylistPlayerAction(
                this, PlaylistPlayerAction.ACTION_SEEK_PLAYITEM, item);
        commandQueue.put(action);
        action.waitEnd();
    }

    private class PlaylistPlayerAction extends BDJAction {
        private PlaylistPlayerAction(Handler player, int action, int param) {
            this.player = player;
            this.action = action;
            this.param = param;
        }

        protected void doAction() {
            switch (action) {
            case ACTION_SEEK_MARK:
                if ((player.getState() == Prefetched) || (player.getState() == Started)) {
                    Libbluray.seekMark(param);
                    player.updateTime(new Time(Libbluray.tellTime() * TO_SECONDS));
                } else if (player.sourceLocator != null) {
                    player.sourceLocator.setMarkId(param);
                    player.sourceLocator.setPlayItemId(-1);
                }
                break;
            case ACTION_SEEK_PLAYITEM:
                if ((player.getState() == Prefetched) || (player.getState() == Started)) {
                    Libbluray.seekPlayItem(param);
                    player.updateTime(new Time(Libbluray.tellTime() * TO_SECONDS));
                } else if (player.sourceLocator != null) {
                    player.sourceLocator.setMarkId(-1);
                    player.sourceLocator.setPlayItemId(param);
                }
                break;
            }
        }

        private Handler player;
        private int action;
        private int param;

        public static final int ACTION_SEEK_MARK = 1;
        public static final int ACTION_SEEK_PLAYITEM = 2;
    }

    private PlaylistInfo pi = null;
    private BDLocator currentLocator = null;
    private BDLocator sourceLocator = null;

    private static final Logger logger = Logger.getLogger(Handler.class.getName());
}
