/*
 * This file is part of libbluray
 * Copyright (C) 2019  VideoLAN
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

package javax.microedition.io;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

public interface Datagram extends DataInput, DataOutput {
    public abstract String getAddress();
    public abstract byte[] getData();
    public abstract int getLength();
    public abstract int getOffset();
    public abstract void reset();
    public abstract void setAddress(String addr) throws IOException;
    public abstract void setAddress(Datagram dgram);
    public abstract void setData(byte[] data, int offset, int len);
    public abstract void setLength(int len);
}
