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

package org.bluray.ti;

public class DiscImpl implements Disc {
    DiscImpl(String id) {
        /* strip leading zeros */
        int i;
        for (i = 0; i < id.length(); i++) {
            if (id.charAt(i) != '0')
                break;
        }
        this.id = id.substring(i);
        if (this.id.length() < 1) {
            this.id = id;
            org.videolan.Logger.getLogger("DiscImpl").error("Invalid Disc ID " + this.id);
        }
    }

    public String getId() {
        /* Returns the 128-bit identifier of this disc (from id.bdmv), without leading zeros.
           Each character in the String represents 4 bits. */
        return id;
    }

    private String id;
}
