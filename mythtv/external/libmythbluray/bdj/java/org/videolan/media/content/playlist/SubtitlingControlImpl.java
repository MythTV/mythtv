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
import java.util.LinkedList;

import org.bluray.media.StreamNotAvailableException;
import org.bluray.media.SubtitleStyleNotAvailableException;
import org.bluray.media.SubtitlingControl;
import org.bluray.media.TextSubtitleNotAvailableException;
import org.bluray.ti.CodingType;
import org.dvb.media.SubtitleListener;
import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class SubtitlingControlImpl extends StreamControl implements SubtitlingControl {
    protected SubtitlingControlImpl(Handler player) {
        super(player);
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
        int psr = Libbluray.readPSR(Libbluray.PSR_PG_STREAM);
        Libbluray.writePSR(Libbluray.PSR_PG_STREAM, (psr & 0xFFFFF000) | num);
    }

    public boolean isSubtitlingOn() {
        return (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x80000000) != 0;
    }

    public boolean setSubtitling(boolean mode) {
        int psr = Libbluray.readPSR(Libbluray.PSR_PG_STREAM);
        boolean oldMode = (psr & 0x80000000) != 0;
        if (mode != oldMode) {
            if (mode)
                Libbluray.writePSR(Libbluray.PSR_PG_STREAM, psr | 0x80000000);
            else
                Libbluray.writePSR(Libbluray.PSR_PG_STREAM, psr & ~0x80000000);
        }
        return oldMode;
    }

    public void selectSubtitle(int subtitle) throws StreamNotAvailableException {
        selectStreamNumber(subtitle);
    }

    public boolean isPipSubtitleMode() {
        return (Libbluray.readPSR(Libbluray.PSR_PG_STREAM) & 0x40000000) != 0;
    }

    public boolean setPipSubtitleMode(boolean mode) {
        int psr = Libbluray.readPSR(Libbluray.PSR_PG_STREAM);
        boolean oldMode = (psr & 0x40000000) != 0;
        if (mode != oldMode) {
            if (mode)
                Libbluray.writePSR(Libbluray.PSR_PG_STREAM, psr | 0x40000000);
            else
                Libbluray.writePSR(Libbluray.PSR_PG_STREAM, psr & ~0x40000000);
        }
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
        synchronized(listeners) {
            listeners.add(listener);
        }
    }

    public void removeSubtitleListener(SubtitleListener listener) {
        synchronized(listeners) {
            listeners.remove(listener);
        }
    }

    protected void onSubtitleChange(int param) {
        synchronized (listeners) {
            if (!listeners.isEmpty())
                BDJActionManager.getInstance().putCallback(
                        new SubtitleCallback(this));
        }
    }

    private class SubtitleCallback extends BDJAction {
        private SubtitleCallback(SubtitlingControlImpl control) {
            this.control = control;
        }

        protected void doAction() {
            LinkedList list;
            synchronized (control.listeners) {
                list = (LinkedList)control.listeners.clone();
            }
            EventObject event = new EventObject(control);
            for (int i = 0; i < list.size(); i++)
                ((SubtitleListener)list.get(i)).subtitleStatusChanged(event);
        }

        private SubtitlingControlImpl control;
    }

    private LinkedList<SubtitleListener> listeners = new LinkedList<SubtitleListener>();
}
