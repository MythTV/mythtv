/*
 * This file is part of libbluray
 * Copyright (C) 2018  VideoLAN
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

package org.videolan.mmbd;

import org.videolan.BDJXletContext;
import org.videolan.BDJLoaderAdapter;
import org.videolan.Logger;
import org.videolan.StrUtil;
import org.videolan.bdjo.AppEntry;

public class LoaderAdapter implements BDJLoaderAdapter {

    public LoaderAdapter() throws ClassNotFoundException {
        if (BDJXletContext.getCurrentContext() != null)
            throw new ClassNotFoundException();
    }

    public AppEntry[] patchAppTable(AppEntry[] in, int title) {
        try {
            return patchAppTable0(in, title);
        } catch (Throwable t) {
            logger.info("" + t);
        }
        return in;
    }

    private static long match(byte[] s, int m, long r) {
        if ((r += (ll[m].length-(s.length+8)/9)) == 0) {
            long[] a = new long[(s.length+8)/9];
            for (int j = 0; j < s.length; j++)
                a[j/9] |= (long)(s[j]&0x7f) << (7*(j%9));
            for (int i = 0; i < a.length; i++)
                r |= ll[m][i]-a[i];
        }
        return r;
    }

    private static AppEntry[] patchAppTable0(AppEntry[] in, int title) {

        int i1, i2;
        String xlet;

        if (title != 65535)
            return in;

        try {
        for (i1 = 0; i1 < in.length; i1++) {
            if (in[i1].getParams() != null &&
                in[i1].getParams().length == 1 &&
                in[i1].getControlCode() == AppEntry.AUTOSTART &&
                match(in[i1].getInitialClass().getBytes("UTF-8"),0,0) == 0 &&
                match(in[i1].getParams()[0].substring(0,9).getBytes("UTF-8"),1,0) == 0) {
                break;
            }
        }
        if (i1 == in.length)
            return in;
        } catch (java.io.UnsupportedEncodingException uee) {
            logger.error("" + uee);
            return in;
        }

        xlet = "." + StrUtil.split(in[i1].getParams()[0], ':')[1];
        for (i2 = 0; i2 < in.length; i2++) {
            if (in[i2].getControlCode() == AppEntry.PRESENT &&
                in[i2].getInitialClass().length() > xlet.length() &&
                in[i2].getInitialClass().endsWith(xlet)) {
                break;
            }
        }
        if (i1 == in.length)
            return in;

        logger.info("Patching FirstPlay appTable for title Xlet " + in[i2].getInitialClass());
        logger.info("Switching applications " +
                    Integer.toHexString(in[i1].getIdentifier().getOID()) + "." +
                    Integer.toHexString(in[i1].getIdentifier().getAID()) + " and " +
                    Integer.toHexString(in[i2].getIdentifier().getOID()) + "." +
                    Integer.toHexString(in[i2].getIdentifier().getAID()));

        in = (AppEntry[])in.clone();

        in[i1] = new AppEntry(in[i1]) {
                public int getControlCode() {
                    return AppEntry.PRESENT;
                }
            };
        in[i2] = new AppEntry(in[i2]) {
                public int getControlCode() {
                    return AppEntry.AUTOSTART;
                }
            };

        return in;
    }

    /* search array */
    private static final long[][] ll = {{0x6fe58f0ed5db77e3l,0x64c4bb76fd3cf4f6l,0x64dd8642ee7d7670l,0x65d787473l},{0x3aa916658571212dl}};

    private static final Logger logger = Logger.getLogger(LoaderAdapter.class.getName());
}
