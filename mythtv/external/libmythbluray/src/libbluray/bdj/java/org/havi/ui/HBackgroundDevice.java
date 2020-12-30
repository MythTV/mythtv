/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2019  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.havi.ui;

import org.blurayx.s3d.ui.HBackgroundConfigTemplateS3D;
import org.blurayx.uhd.ui.HBackgroundConfigurationTemplateUHD;

import java.awt.Color;

public class HBackgroundDevice extends HScreenDevice {
    protected HBackgroundDevice() {
        boolean is_p6 = isProfile6();
        boolean is_p5 = isProfile5();
        int length = HScreenConfigTemplate.defaultConfig.length;
        hbcArray = new HBackgroundConfiguration[length];
        for (int i = 0; i < length; i++) {
            HBackgroundConfigTemplate hbct;
            if (is_p6) {
                hbct = new HBackgroundConfigurationTemplateUHD();
            } else if (is_p5) {
                hbct = new HBackgroundConfigTemplateS3D();
            } else {
                hbct = new HBackgroundConfigTemplate();
            }
            HScreenConfigTemplate.initDefaultConfigTemplate(hbct, i);
            hbcArray[i] = new HStillImageBackgroundConfiguration(hbct, new Color(0, 0, 0, 0));
        }
        hbc = hbcArray[0];
    }

    public HBackgroundConfiguration[] getConfigurations() {
        return hbcArray;
    }

    public HBackgroundConfiguration getDefaultConfiguration() {
        return hbcArray[0];
    }

    public HBackgroundConfiguration getBestConfiguration(HBackgroundConfigTemplate hbct) {
        int score = -1;
        HBackgroundConfiguration hbc = null;
        for (int i = 0; i < hbcArray.length; i++)
            if (hbct.match(hbcArray[i]) > score)
                hbc = hbcArray[i];
        return hbc;
    }

    public HBackgroundConfiguration getBestConfiguration(HBackgroundConfigTemplate hbcta[]) {
        int score = -1;
        HBackgroundConfiguration hbc = null;
        for (int i = 0; i < hbcArray.length; i++)
            for (int j = 0; j < hbcta.length; j++)
                if (hbcta[j].match(hbcArray[i]) > score)
                    hbc = hbcArray[i];
        return hbc;
    }

    public HBackgroundConfiguration getCurrentConfiguration() {
        return hbc;
    }

    public boolean setBackgroundConfiguration(HBackgroundConfiguration hbc)
            throws SecurityException, HPermissionDeniedException,
            HConfigurationException {
        this.hbc = hbc;
        return true;
    }

    private HBackgroundConfiguration[] hbcArray;
    private HBackgroundConfiguration hbc;
}
