/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.videolan.media.protocol.file;

import java.io.IOException;
import javax.media.Time;

public class DataSource extends javax.media.protocol.DataSource {
    public Object[] getControls() {
        return new Object[0];
    }

    public Object getControl(String controlType) {
        return null;
    }

    public Time getDuration() {
        org.videolan.Logger.unimplemented(DataSource.class.getName(), "getDuration");
        return DURATION_UNKNOWN;
    }

    public String getContentType() {
        return contentType;
    }

    public void connect() throws IOException {
        contentType = "audio";
    }

    public void disconnect() {
        contentType = "unknown";
    }

    public void start() throws IOException {
        if (contentType.equals("unknown"))
            throw new IOException("Unknown content type.");
    }

    public void stop() throws IOException {
    }

    private String contentType = "unknown";
}
