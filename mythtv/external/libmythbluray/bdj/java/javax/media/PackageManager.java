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

package javax.media;

import java.util.Vector;

public class PackageManager {
    public PackageManager()
    {
    }

    public static Vector<?> getProtocolPrefixList()
    {
        return protocolPrefix;
    }

    public static void setProtocolPrefixList(Vector<?> list)
    {
        protocolPrefixTemp = list;
    }

	public static void commitProtocolPrefixList()
    {
        SecurityManager sec = System.getSecurityManager();
        if (sec != null)
            sec.checkPropertiesAccess();
        protocolPrefix = (Vector<?>) protocolPrefixTemp.clone();
    }

	public static Vector<?> getContentPrefixList()
    {
        return contentPrefix;
    }

    public static void setContentPrefixList(Vector<?> list)
    {
        contentPrefixTemp = list;
    }

    public static void commitContentPrefixList()
    {
        SecurityManager sec = System.getSecurityManager();
        if (sec != null)
            sec.checkPropertiesAccess();
        contentPrefix = (Vector<?>) contentPrefixTemp.clone();
    }
    
    private static Vector<?> protocolPrefixTemp = null;
    private static Vector<?> contentPrefixTemp = null;
    private static Vector<?> protocolPrefix = null;
    private static Vector<?> contentPrefix = null;
}
