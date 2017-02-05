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

class BDGraphicsDevice extends BDGraphicsDeviceImpl {

    BDGraphicsDevice(BDGraphicsEnvironment environment) {
        super(environment);
    }

    /* J2SE java 7 */
    /*
    public boolean isWindowTranslucencySupported(GraphicsDevice.WindowTranslucency translucencyKind) {
        return true;
    }
    */
}
