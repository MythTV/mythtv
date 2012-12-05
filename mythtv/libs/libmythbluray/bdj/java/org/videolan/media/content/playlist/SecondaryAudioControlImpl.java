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

import org.bluray.media.SecondaryAudioControl;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class SecondaryAudioControlImpl extends StreamControl implements SecondaryAudioControl {
    protected SecondaryAudioControlImpl(Handler player) {
        super(player);
    }

    protected StreamInfo[] getStreams() {
        TIClip ci = player.getCurrentClipInfo();
        if (ci == null)
            return null;
        return ci.getSecAudioStreams();
    }

    protected String getDefaultLanguage() {
        return languageFromInteger(Libbluray.readPSR(Libbluray.PSR_AUDIO_LANG));
    }

    public int getCurrentStreamNumber() {
        return Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO) & 0x000000FF;
    }

    protected void setStreamNumber(int num) {
        int psr = Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO);
        Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, (psr & 0xFFFFFF00) | num);
    }
}
