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

import javax.media.Control;

import org.bluray.media.StreamNotAvailableException;
import org.davic.media.LanguageNotAvailableException;
import org.davic.media.NotAuthorizedException;
import org.videolan.StreamInfo;

public abstract class StreamControl implements Control {
    protected StreamControl(Handler player) {
        this.player = player;
    }

    protected abstract StreamInfo[] getStreams();

    protected abstract void setStreamNumber(int num);

    protected String getDefaultLanguage() {
        return "";
    }

    protected StreamInfo getCurrentStream() {
        StreamInfo[] streams = getStreams();
        int stream = getCurrentStreamNumber();
        if ((streams == null) || (stream <= 0) || (stream > streams.length))
            return null;
        return streams[stream - 1];
    }

    protected String languageFromInteger(int value) {
        char[] language = new char[3];
        language[0] = (char)(value >> 16);
        language[1] = (char)(value >> 8);
        language[2] = (char)value;
        return String.valueOf(language);
    }

    public Component getControlComponent() {
        return null;
    }

    public abstract int getCurrentStreamNumber();

    public int[] listAvailableStreamNumbers() {
        StreamInfo[] streams = getStreams();
        if (streams == null)
            return new int[0];
        int[] ret = new int[streams.length];
        for (int i = 0; i < streams.length; i++)
            ret[i] = i + 1;
        return ret;
    }

    public void selectStreamNumber(int num) throws StreamNotAvailableException {
        if (num < 1)
            throw new StreamNotAvailableException();
        StreamInfo[] streams = getStreams();
        if ((streams == null) || (num > streams.length))
            throw new StreamNotAvailableException();
        setStreamNumber(num);
    }

    public String[] listAvailableLanguages() {
        StreamInfo[] streams = getStreams();
        if (streams == null)
            return new String[0];
        String[] ret = new String[streams.length];
        for (int i = 0; i < streams.length; i++)
            ret[i] = streams[i].getLang();
        return ret;
    }

    public String getCurrentLanguage() {
        StreamInfo stream = getCurrentStream();
        if (stream == null)
            return "";
        return stream.getLang();
    }

    public String selectDefaultLanguage() throws NotAuthorizedException {
        String language = getDefaultLanguage();
        try {
            selectLanguage(language);
        } catch (LanguageNotAvailableException e) {
            throw new NotAuthorizedException();
        }
        return language;
    }

    public void selectLanguage(String language)
            throws LanguageNotAvailableException, NotAuthorizedException {
        StreamInfo[] streams = getStreams();
        if (streams == null)
            throw new NotAuthorizedException();
        for (int i = 0; i < streams.length; i++) {
            if (streams[i].getLang().equals(language)) {
                try {
                    selectStreamNumber(i + 1);
                } catch (StreamNotAvailableException e) {
                    throw new NotAuthorizedException();
                }
            }
        }
        throw new LanguageNotAvailableException();
    }

    protected Handler player;
}
