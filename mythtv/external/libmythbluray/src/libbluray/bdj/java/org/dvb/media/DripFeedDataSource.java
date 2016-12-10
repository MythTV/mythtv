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

package org.dvb.media;

import javax.media.MediaLocator;
import javax.media.Time;
import java.io.IOException;
import javax.media.protocol.DataSource;

public class DripFeedDataSource extends DataSource {
    public DripFeedDataSource() {
        SecurityManager security = System.getSecurityManager();
        if (security != null)
            security.checkPermission(new DripFeedPermission("*"));
    }

    public DripFeedDataSource(MediaLocator source) {
        SecurityManager security = System.getSecurityManager();
        if (security != null)
            security.checkPermission(new DripFeedPermission("*"));
        setLocator(source);
    }

    public void feed(byte[] clip_part) {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "feed");
        throw new Error("Not implemented");
    }

    public String getContentType() {
        return "video/dvb.mpeg.drip";
    }

    public void connect() throws IOException {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "connect");
        throw new Error("Not implemented");
    }

    public void disconnect() {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "disconnect");
        throw new Error("Not implemented");
    }

    public void start() throws IOException {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "start");
        throw new Error("Not implemented");
    }

    public void stop() throws IOException {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "stop");
        throw new Error("Not implemented");
    }

    public Time getDuration() {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "getDuration");
        throw new Error("Not implemented");
    }

    public Object[] getControls() {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "getControls");
        throw new Error("Not implemented");
    }

    public Object getControl(String controlType) {
        org.videolan.Logger.unimplemented(DripFeedDataSource.class.getName(), "getControls");
        throw new Error("Not implemented");
    }

    public void setLocator(MediaLocator source) {
        if ((source != null) && ("dripfeed://".equalsIgnoreCase(source.toExternalForm())))
            super.setLocator(source);
    }
}
