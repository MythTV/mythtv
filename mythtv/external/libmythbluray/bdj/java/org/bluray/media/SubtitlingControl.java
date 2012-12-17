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

package org.bluray.media;

import org.davic.media.LanguageNotAvailableException;
import org.davic.media.NotAuthorizedException;
import org.dvb.media.SubtitlingEventControl;
import org.bluray.ti.CodingType;

public abstract interface SubtitlingControl extends SubtitlingEventControl {
    public abstract String selectDefaultLanguage()
            throws NotAuthorizedException;

    public abstract void selectLanguage(String language)
            throws LanguageNotAvailableException, NotAuthorizedException;

    public abstract void selectStreamNumber(int num)
            throws StreamNotAvailableException;

    @Deprecated
    public abstract void selectSubtitle(int subtitle)
            throws StreamNotAvailableException;

    public abstract boolean setPipSubtitleMode(boolean mode);

    public abstract void setSubtitleStyle(int style)
            throws TextSubtitleNotAvailableException,
            SubtitleStyleNotAvailableException;

    public abstract String getCurrentLanguage();

    public abstract String[] listAvailableLanguages();

    public abstract int getCurrentStreamNumber();

    public abstract int[] listAvailableStreamNumbers();

    public abstract CodingType getCurrentSubtitleType();

    public abstract int getSubtitleStyle()
            throws TextSubtitleNotAvailableException,
            SubtitleStyleNotAvailableException;

    public abstract boolean isPipSubtitleMode();
}
