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

import java.awt.Dimension;

import javax.tv.xlet.XletContext;

import org.davic.resources.ResourceClient;
import org.davic.resources.ResourceProxy;
import org.davic.resources.ResourceServer;
import org.davic.resources.ResourceStatusListener;
import org.havi.ui.event.HScreenConfigurationEvent;
import org.havi.ui.event.HScreenConfigurationListener;
import org.havi.ui.event.HScreenDeviceReleasedEvent;
import org.havi.ui.event.HScreenDeviceReservedEvent;

import org.videolan.BDJXletContext;

public class HScreenDevice implements ResourceProxy, ResourceServer {
    HScreenDevice() {
    }

    public String getIDstring() {
        return "HAVi Screen Device";
    }

    public void addScreenConfigurationListener(HScreenConfigurationListener hscl) {
        this.hscl = HEventMulticaster.add(this.hscl, hscl);
    }

    public void addScreenConfigurationListener(HScreenConfigurationListener hscl, HScreenConfigTemplate hsct) {
        this.hscl = HEventMulticaster.add(this.hscl, hscl, hsct);
    }

    public void removeScreenConfigurationListener(HScreenConfigurationListener hscl) {
        this.hscl = HEventMulticaster.remove(this.hscl, hscl);
    }

    public Dimension getScreenAspectRatio() {
        throw new Error("Not implemented");
    }

    public boolean reserveDevice(ResourceClient client) {
        if (this.client == client)
            return true;
        if (this.client != null) {
            if (!this.client.requestRelease(this, null))
                return false;
        }
        context = BDJXletContext.getCurrentContext();
        this.client = client;
        if (listener != null)
            listener.statusChanged(new HScreenDeviceReservedEvent(client));
        return true;
    }

    public void releaseDevice() {
        if (context != BDJXletContext.getCurrentContext())
            return;
        if (listener != null)
            listener.statusChanged(new HScreenDeviceReleasedEvent(client));
        context = null;
        client = null;
    }

    public ResourceClient getClient() {
        return client;
    }

    public void addResourceStatusEventListener(ResourceStatusListener listener) {
        this.listener = HEventMulticaster.add(this.listener, listener);
    }

    public void removeResourceStatusEventListener(ResourceStatusListener listener) {
        this.listener = HEventMulticaster.remove(this.listener, listener);
    }

    protected void reportScreenConfigurationEvent(HScreenConfigurationEvent evt) {
        if (hscl != null)
            hscl.report(evt);
    }

    protected void testRight() throws HPermissionDeniedException {
        if (context != BDJXletContext.getCurrentContext())
            throw new HPermissionDeniedException();
    }

    private HScreenConfigurationListener hscl = null;
    private ResourceStatusListener listener = null;
    private XletContext context = null;
    private ResourceClient client = null;
}
