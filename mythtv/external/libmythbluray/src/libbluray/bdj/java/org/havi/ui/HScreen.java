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

public class HScreen {
    private HScreen() {
        hVideoDevice = new HVideoDevice();
        hGraphicsDevice = new HGraphicsDevice();
        hBackgroundDevice = new HBackgroundDevice();
    }

    public static HScreen[] getHScreens() {
         HScreen[] screens = new HScreen[1];
         screens[0] = DEFAULT_HSCREEN;
         return screens;
    }

    public static HScreen getDefaultHScreen() {
        return DEFAULT_HSCREEN;
    }

    public HVideoDevice[] getHVideoDevices() {
        HVideoDevice[] devices = new HVideoDevice[1];
        devices[0] = hVideoDevice;
        return devices;
    }

    public HVideoDevice getDefaultHVideoDevice() {
        return hVideoDevice;
    }

    public HVideoConfiguration getBestConfiguration(HVideoConfigTemplate[] hvcta) {
        return hVideoDevice.getBestConfiguration(hvcta);
    }

    public HGraphicsDevice[] getHGraphicsDevices() {
        HGraphicsDevice[] devices = new HGraphicsDevice[1];
        devices[0] = hGraphicsDevice;
        return devices;
    }

    public HGraphicsDevice getDefaultHGraphicsDevice() {
        return hGraphicsDevice;
    }

    public HGraphicsConfiguration getBestConfiguration(
            HGraphicsConfigTemplate[] hgcta) {
        return hGraphicsDevice.getBestConfiguration(hgcta);
    }

    public HBackgroundDevice[] getHBackgroundDevices() {
        HBackgroundDevice[] devices = new HBackgroundDevice[1];
        devices[0] = hBackgroundDevice;
        return devices;
    }

    public HBackgroundDevice getDefaultHBackgroundDevice() {
        return hBackgroundDevice;
    }

    public HBackgroundConfiguration getBestConfiguration(
            HBackgroundConfigTemplate[] hbcta) {
        return hBackgroundDevice.getBestConfiguration(hbcta);
    }

    public HScreenConfiguration[] getCoherentScreenConfigurations(
            HScreenConfigTemplate[] hscta) {

        if ((hscta == null) || (hscta.length == 0))
            throw new IllegalArgumentException("HScreenConfigTemplate[] hscta cannot be null");

        HScreenConfiguration[] hsc = new HScreenConfiguration[hscta.length];

        for (int i = 0; i < hscta.length; i++) {
            if ((hscta[i] instanceof HVideoConfigTemplate))
                hsc[i] = hVideoDevice.getBestConfiguration((HVideoConfigTemplate)hscta[i]);
            else if ((hscta[i] instanceof HGraphicsConfigTemplate))
                hsc[i] = hGraphicsDevice.getBestConfiguration((HGraphicsConfigTemplate)hscta[i]);
            else if ((hscta[i] instanceof HBackgroundConfigTemplate))
                hsc[i] = hBackgroundDevice.getBestConfiguration((HBackgroundConfigTemplate)hscta[i]);
            else
              return null;
        }

        return hsc;
    }

    public boolean setCoherentScreenConfigurations(HScreenConfiguration[] hsca)
            throws java.lang.SecurityException, HPermissionDeniedException,
            HConfigurationException {

        if ((hsca == null) || (hsca.length == 0))
            throw new IllegalArgumentException("HScreenConfiguration[] hsca cannot be null");

        for (int i = 0; i < hsca.length; i++) {
            if (hsca[i] instanceof HVideoConfiguration) {
                if (!((HVideoConfiguration)hsca[i]).getDevice().setVideoConfiguration((HVideoConfiguration)hsca[i]))
                    return false;
            }
            else if (hsca[i] instanceof HGraphicsConfiguration) {
                if (!((HGraphicsConfiguration)hsca[i]).getDevice().setGraphicsConfiguration((HGraphicsConfiguration)hsca[i]))
                    return false;
            }
            else if (hsca[i] instanceof HBackgroundConfiguration) {
                if (!((HBackgroundConfiguration)hsca[i]).getDevice().setBackgroundConfiguration((HBackgroundConfiguration)hsca[i]))
                    return false;
            }
        }

        return true;
    }

    private static final HScreen DEFAULT_HSCREEN = new HScreen();
    private HVideoDevice hVideoDevice;
    private HGraphicsDevice hGraphicsDevice;
    private HBackgroundDevice hBackgroundDevice;
}
