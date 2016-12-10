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

public class HVideoDevice extends HScreenDevice {
    protected HVideoDevice() {
        int length = HScreenConfigTemplate.defaultConfig.length;
        hvcArray = new HVideoConfiguration[length];
        for (int i = 0; i < length; i++) {
            HVideoConfigTemplate hvct = new HVideoConfigTemplate();
            HScreenConfigTemplate.initDefaultConfigTemplate(hvct, i);
            hvcArray[i] = new HVideoConfiguration(hvct);
        }
        hvc = hvcArray[0];
    }

    public HVideoConfiguration[] getConfigurations() {
        return hvcArray;
    }

    public HVideoConfiguration getDefaultConfiguration() {
        return hvcArray[0];
    }

    public HVideoConfiguration getBestConfiguration(HVideoConfigTemplate hvct) {
        int score = -1;
        HVideoConfiguration hvc = null;
        for (int i = 0; i < hvcArray.length; i++)
            if (hvct.match(hvcArray[i]) > score)
                hvc = hvcArray[i];
        return hvc;
    }

    public HVideoConfiguration getBestConfiguration(HVideoConfigTemplate hvcta[]) {
        int score = -1;
        HVideoConfiguration hvc = null;
        for (int i = 0; i < hvcArray.length; i++) 
            for (int j = 0; j < hvcta.length; j++)
                if (hvcta[j].match(hvcArray[i]) > score)
                    hvc = hvcArray[i];
        return hvc;
    }

    public HVideoConfiguration getCurrentConfiguration() {
        return hvc;
    }

    public boolean setVideoConfiguration(HVideoConfiguration hvc)
            throws SecurityException, HPermissionDeniedException, HConfigurationException {
        this.hvc = hvc;
        return true;
    }

    public Object getVideoSource() throws SecurityException, HPermissionDeniedException {
        org.videolan.Logger.unimplemented(HVideoDevice.class.getName(), "getVideoSource");
        throw new HPermissionDeniedException();
    }

    public Object getVideoController() throws SecurityException, HPermissionDeniedException {
        org.videolan.Logger.unimplemented(HVideoDevice.class.getName(), "getVideoController");
        throw new HPermissionDeniedException();
    }

    public static final HVideoConfiguration NOT_CONTRIBUTING = null;
    private HVideoConfiguration[] hvcArray;
    private HVideoConfiguration hvc;
}
