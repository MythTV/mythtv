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
package org.dvb.dsmcc;

import java.io.IOException;
import org.davic.net.Locator;

public class DSMCCStream {
    public DSMCCStream(DSMCCObject aDSMCCObject) throws NotLoadedException,
            IllegalObjectTypeException
    {
        throw new Error("Not implemented");
    }

    public DSMCCStream(String path) throws IOException,
            IllegalObjectTypeException
    {
        throw new Error("Not implemented");
    }

    public DSMCCStream(String path, String name) throws IOException,
            IllegalObjectTypeException
    {
        throw new Error("Not implemented");
    }

    public long getDuration()
    {
        throw new Error("Not implemented");
    }

    public long getNPT() throws MPEGDeliveryException
    {
        throw new Error("Not implemented");
    }

    public Locator getStreamLocator()
    {
        throw new Error("Not implemented");
    }

    public boolean isMPEGProgram()
    {
        throw new Error("Not implemented");
    }

    public boolean isAudio()
    {
        throw new Error("Not implemented");
    }

    public boolean isVideo()
    {
        throw new Error("Not implemented");
    }

    public boolean isData()
    {
        throw new Error("Not implemented");
    }

    public NPTRate getNPTRate() throws MPEGDeliveryException
    {
        throw new Error("Not implemented");
    }

    public void addNPTListener(NPTListener listener)
    {
        throw new Error("Not implemented");
    }

    public void removeNPTListener(NPTListener listener)
    {
        throw new Error("Not implemented");
    }
}
