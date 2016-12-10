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

import java.util.EventObject;

import org.bluray.media.StreamNotAvailableException;
import org.bluray.media.SubtitleStyleNotAvailableException;
import org.bluray.media.SubtitlingControl;
import org.bluray.media.TextSubtitleNotAvailableException;
import org.bluray.ti.CodingType;
import org.dvb.media.SubtitleAvailableEvent;
import org.dvb.media.SubtitleListener;
import org.dvb.media.SubtitleNotAvailableEvent;
import org.dvb.media.SubtitleNotSelectedEvent;
import org.dvb.media.SubtitleSelectedEvent;
import org.videolan.BDJListeners;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class SubtitlingControlImpl extends StreamControl implements SubtitlingControl {
    protected SubtitlingControlImpl(Handler player) {
        this.player = player;
    }

    protected StreamInfo[] getStreams() {
        TIClip ci = player.getCurrentClipInfo();
        if (ci == null)
            return null;
        return ci.getPgStreams();
    }

    protected String getDefaultLanguage() {
        return languageFromInteger(Libbluray.readPSR(Libbluray.PSR_PG_AND_SUB_LANG));
    }

    public int getCurrentStreamNumber() {
        return Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x00000FFF;
    }

    protected void setStreamNumber(int num) {
        Libbluray.writePSR(Libbluray.PSR_PG_STREAM, num, 0x00000fff);
    }

    public boolean isSubtitlingOn() {
        return (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x80000000) != 0;
    }

    public boolean setSubtitling(boolean mode) {
        boolean oldMode = (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x80000000) != 0;
        Libbluray.writePSR(Libbluray.PSR_PG_STREAM, mode ? 0x80000000 : 0, 0x80000000);
        return oldMode;
    }

    public void selectSubtitle(int subtitle) throws StreamNotAvailableException {
        selectStreamNumber(subtitle);
    }

    public boolean isPipSubtitleMode() {
        return (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x40000000) != 0;
    }

    public boolean setPipSubtitleMode(boolean mode) {
        boolean oldMode = (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x40000000) != 0;
        Libbluray.writePSR(Libbluray.PSR_PG_STREAM, mode ? 0x40000000 : 0, 0x40000000);
        return oldMode;
    }

    public void setSubtitleStyle(int style)
        throws TextSubtitleNotAvailableException, SubtitleStyleNotAvailableException {
        if ((style <= 0) || ((style > 25) && (style != 255)))
            throw new SubtitleStyleNotAvailableException();
        if (getCurrentSubtitleType() != CodingType.TEXT_SUBTITLE)
            throw new TextSubtitleNotAvailableException();
        Libbluray.writePSR(Libbluray.PSR_STYLE, style);
    }

    public CodingType getCurrentSubtitleType() {
        StreamInfo[] streamInfo = getStreams();
        if (streamInfo == null)
            return null;
        int stream = getCurrentStreamNumber();
        if ((stream <= 0) || (stream > streamInfo.length))
            return null;
        return streamInfo[stream - 1].getCodingType();
    }

    public int getSubtitleStyle()
        throws TextSubtitleNotAvailableException, SubtitleStyleNotAvailableException {
        if (getCurrentSubtitleType() != CodingType.TEXT_SUBTITLE)
            throw new TextSubtitleNotAvailableException();
        int style = Libbluray.readPSR(Libbluray.PSR_STYLE);
        if ((style <= 0) || ((style > 25) && (style != 255)))
            throw new SubtitleStyleNotAvailableException();
        return style;
    }

    public void addSubtitleListener(SubtitleListener listener) {
        listeners.add(listener);
    }

    public void removeSubtitleListener(SubtitleListener listener) {
        listeners.remove(listener);
    }

    protected void onSubtitleChange(int param) {
        if ((param & 0x80000000) != 0) {
            listeners.putCallback(new SubtitleSelectedEvent(this));
        } else {
            listeners.putCallback(new SubtitleNotSelectedEvent(this));
        }
    }

    protected void onSubtitleAvailable(boolean param) {
        if (param) {
            listeners.putCallback(new SubtitleAvailableEvent(this));
        } else {
            listeners.putCallback(new SubtitleNotAvailableEvent(this));
        }
    }

    private Handler player;
    private BDJListeners listeners = new BDJListeners();
}
