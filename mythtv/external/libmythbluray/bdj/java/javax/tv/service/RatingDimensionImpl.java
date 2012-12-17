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

package javax.tv.service;

import javax.tv.service.SIException;

public class RatingDimensionImpl implements RatingDimension {
    public String getDimensionName() {
        return dimensionName;
    }

    public short getNumberOfLevels() {
        return 255;
    }

    public String[] getRatingLevelDescription(short level) throws SIException {
        if ((level < 0) || (level >= 255))
            throw new SIException();
        String[] description = new String[2];
        description[0] = description[1] = Short.toString(level);
        return description;
    }

    public static final String dimensionName = "BD_ROM_PARENTAL_LOCK";
}
