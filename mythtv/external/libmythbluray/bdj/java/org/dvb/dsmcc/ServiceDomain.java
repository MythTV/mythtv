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

import java.io.FileNotFoundException;
import java.io.InterruptedIOException;
import java.net.URL;
import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.davic.net.Locator;
import org.videolan.MountException;
import org.videolan.MountManager;

public class ServiceDomain {
    public ServiceDomain()
    {

    }

    public void attach(Locator dvbService, int carouselId)
            throws ServiceXFRException, InterruptedIOException,
            MPEGDeliveryException
    {
        throw new Error("Not implemented");
    }

    public void attach(Locator locator) throws DSMCCException,
            InterruptedIOException, MPEGDeliveryException
    {   
        BDLocator bdl = checkLocator(locator);
        if (bdl == null)
            throw new DSMCCException("invalid BDLocator");
        
        if (!bdl.isJarFileItem())
            throw new DSMCCException("invalid BDLocator");
       
        try {
            this.mountPoint = new DSMCCObject(MountManager.mount(bdl.getJarFileId()));
        } catch (MountException e) {
            e.printStackTrace();
            throw new DSMCCException("couldn't mount jar");
        }
    }

    public void attach(byte[] NSAPAddress) throws DSMCCException,
            InterruptedIOException, InvalidAddressException,
            MPEGDeliveryException
    {
        throw new Error("Not implemented");
    }

    public void detach() throws NotLoadedException
    {
        if (mountPoint == null)
            throw new NotLoadedException();
    }

    public byte[] getNSAPAddress() throws NotLoadedException
    {
        throw new Error("Not implemented");
    }

    public static URL getURL(Locator locator)
            throws NotLoadedException, InvalidLocatorException,
            FileNotFoundException
    {
        BDLocator bdl = checkLocator(locator);
        if (bdl == null)
            throw new InvalidLocatorException("invalid BDLocator");
        
        if (bdl.isJarFileItem()) {
            String mountPt = MountManager.getMount(bdl.getJarFileId());
            
            if (mountPt == null)
                throw new NotLoadedException();
            
            DSMCCObject obj = new DSMCCObject(mountPt, bdl.getPathSegments());
            
            if (!obj.exists())
                throw new FileNotFoundException();
            
            return obj.getURL();
        }
        
        throw new InvalidLocatorException();
    }

    public DSMCCObject getMountPoint()
    {
        return mountPoint;
    }

    public boolean isNetworkConnectionAvailable()
    {
        return false;
    }

    public boolean isAttached()
    {
        return mountPoint != null;
    }

    public Locator getLocator()
    {
        return locator;
    }
    
    private static BDLocator checkLocator(Locator locator)
    {
        if (!(locator instanceof BDLocator))
            return null;
        
        BDLocator bdl = (BDLocator)locator;
        
        if (bdl.isJarFileItem())
            return bdl;
        else
            return null;
    }
    
    private DSMCCObject mountPoint = null;
    private Locator locator = null;
}
