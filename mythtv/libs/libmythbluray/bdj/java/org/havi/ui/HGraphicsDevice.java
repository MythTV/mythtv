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

package org.havi.ui;

public class HGraphicsDevice extends HScreenDevice {
    protected HGraphicsDevice() {
        int length = HScreenConfigTemplate.defaultConfig.length;
        hgcArray = new HGraphicsConfiguration[length];
        for (int i = 0; i < length; i++) {
            HGraphicsConfigTemplate hgct = new HGraphicsConfigTemplate();
            HScreenConfigTemplate.initDefaultConfigTemplate(hgct, i);
            hgcArray[i] = new HGraphicsConfiguration(hgct);
        }
        hgc = hgcArray[0];
    }

    public HGraphicsConfiguration[] getConfigurations() {
        return hgcArray;
    }

    public HGraphicsConfiguration getDefaultConfiguration() {
        return hgcArray[0];
    }

    public HGraphicsConfiguration getBestConfiguration(HGraphicsConfigTemplate hgct) {
        int score = -1;
        HGraphicsConfiguration hgc = null;
        for (int i = 0; i < hgcArray.length; i++)
            if (hgct.match(hgcArray[i]) > score)
                hgc = hgcArray[i];
        return hgc;
    }

    public HGraphicsConfiguration getBestConfiguration(HGraphicsConfigTemplate hgcta[]) {
        int score = -1;
        HGraphicsConfiguration hgc = null;
        for (int i = 0; i < hgcArray.length; i++)
            for (int j = 0; j < hgcta.length; j++)
                if (hgcta[j].match(hgcArray[i]) > score)
                    hgc = hgcArray[i];
        return hgc;
    }

    public HGraphicsConfiguration getCurrentConfiguration() {
        return hgc;
    }

    public boolean setGraphicsConfiguration(HGraphicsConfiguration hgc)
            throws SecurityException, HPermissionDeniedException,
            HConfigurationException {
        this.hgc = hgc;
        return true;
    }

    private HGraphicsConfiguration[] hgcArray;
    private HGraphicsConfiguration hgc;
}
