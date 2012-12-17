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

import java.io.File;
import java.io.InterruptedIOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.cert.X509Certificate;
import java.util.logging.Logger;

public class DSMCCObject extends File {
    public DSMCCObject(String path)
    {
        super(path);
    }

    public DSMCCObject(String path, String name)
    {
        super(path, name);

    }

    public DSMCCObject(DSMCCObject dir, String name)
    {
        super(dir.getPath(), name);
    }

    public boolean isLoaded()
    {
        return loaded;
    }

    public boolean isStream()
    {
        return stream;
    }

    public boolean isStreamEvent()
    {
        return streamEvent;
    }

    public boolean isObjectKindKnown()
    {
        return true;
    }

    public void synchronousLoad() throws InvalidFormatException,
            InterruptedIOException, MPEGDeliveryException,
            ServerDeliveryException, InvalidPathNameException,
            NotEntitledException, ServiceXFRException,
            InsufficientResourcesException
    {
        if (!super.exists())
            throw new InvalidPathNameException();
        
        this.loaded = true;
    }

    public void asynchronousLoad(AsynchronousLoadingEventListener listener)
            throws InvalidPathNameException
    {
        try {
            synchronousLoad();
            
            listener.receiveEvent(new SuccessEvent(this));
        } catch (DSMCCException e) {
            // never really thrown so don't care
        } catch (InterruptedIOException e) {
            // never really thrown so don't care
        }
    }

    public void abort() throws NothingToAbortException
    {
        throw new NothingToAbortException();
    }

    public static boolean prefetch(String path, byte priority)
    {
        return false;
    }

    public static boolean prefetch(DSMCCObject dir, String path, byte priority)
    {
        return false;
    }

    public void unload() throws NotLoadedException
    {
        if (loaded)
            throw new NotLoadedException();
        
        loaded = false;
    }

    public URL getURL()
    {
        String url = "file://" + super.getAbsolutePath();
        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            logger.warning("Failed to make url " + url);
            e.printStackTrace();
            return null;
        }
    }

    public void addObjectChangeEventListener(ObjectChangeEventListener listener)
            throws InsufficientResourcesException
    {
        throw new Error("Not implemented"); // NOTE: probably unnecessary
    }

    public void removeObjectChangeEventListener(
            ObjectChangeEventListener listener)
    {
        throw new Error("Not implemented"); // NOTE: probably unnecessary
    }

    public void loadDirectoryEntry(AsynchronousLoadingEventListener listener)
            throws InvalidPathNameException
    {
        if (!super.exists())
            throw new InvalidPathNameException();
        
        listener.receiveEvent(new SuccessEvent(this));
    }

    public void setRetrievalMode(int retrieval_mode)
    {
        throw new Error("Not implemented");
    }

    public X509Certificate[][] getSigners()
    {
        throw new Error("Not implemented");
    }

    public X509Certificate[][] getSigners(boolean known_root)
            throws InvalidFormatException, InterruptedIOException,
            MPEGDeliveryException, ServerDeliveryException,
            InvalidPathNameException, NotEntitledException,
            ServiceXFRException, InsufficientResourcesException
    {
        throw new Error("Not implemented");
    }

    public static final int FROM_CACHE = 1;
    public static final int FROM_CACHE_OR_STREAM = 2;
    public static final int FROM_STREAM_ONLY = 3;
    public static final int FORCED_STATIC_CACHING = 4;

    private boolean loaded = false;
    private boolean stream = false;
    private boolean streamEvent = false;

    private static final long serialVersionUID = -6845145080873848152L;

    private static final Logger logger = Logger.getLogger(DSMCCObject.class.getName());
}
