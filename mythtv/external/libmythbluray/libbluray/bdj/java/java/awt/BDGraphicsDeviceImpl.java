/*
 * This file is part of libbluray
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

package java.awt;

class BDGraphicsDeviceImpl extends GraphicsDevice {
    private BDGraphicsConfiguration configuration;

    BDGraphicsDeviceImpl(BDGraphicsEnvironment environment) {
        configuration = new BDGraphicsConfiguration((BDGraphicsDevice)this);
    }

    public int getType() {
        return TYPE_RASTER_SCREEN;
    }

    public String getIDstring() {
        return "BDJ Graphics Device";
    }

    public GraphicsConfiguration getDefaultConfiguration() {
        return configuration;
    }

    public GraphicsConfiguration[] getConfigurations() {
        return new GraphicsConfiguration[] { configuration };
    }

    Rectangle getBounds() {
        return new Rectangle(1920, 1080);
    }

    public int getAvailableAcceleratedMemory() {
        return 0;
    }

    public boolean isFullScreenSupported() {
        return false;
    }

    public Window getFullScreenWindow() {
        return null;
    }

    public void setFullScreenWindow(Window window) {
    }

    boolean isWindowPerpixelTranslucencySupported() {
        return true;
    }
}
