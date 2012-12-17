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

package org.davic.mpeg;

public interface NotAuthorizedInterface {
    public int getType();
    
    //public Service getService();
    
    //public ElementaryStream[] getElementaryStreams();

    public int[] getReason(int index)
            throws java.lang.IndexOutOfBoundsException;

    public final static int POSSIBLE_UNDER_CONDITIONS = 0;
    public final static int NOT_POSSIBLE = 1;

    public final static int COMMERCIAL_DIALOG = 1;
    public final static int MATURITY_RATING_DIALOG = 2;
    public final static int TECHNICAL_DIALOG = 3;
    public final static int FREE_PREVIEW_DIALOG = 4;

    public final static int NO_ENTITLEMENT = 1;
    public final static int MATURITY_RATING = 2;
    public final static int TECHNICAL = 3;
    public final static int GEOGRAPHICAL_BLACKOUT = 4;
    public final static int OTHER = 5;

    public final static int SERVICE = 0;
    public final static int ELEMENTARY_STREAM = 1;

}
