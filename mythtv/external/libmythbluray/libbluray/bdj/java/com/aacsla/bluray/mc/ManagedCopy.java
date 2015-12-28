/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package com.aacsla.bluray.mc;

import java.net.URL;

import org.videolan.Logger;

public final class ManagedCopy {
    public static ManagedCopy getInstance() throws MCException {
        logger.unimplemented("*");
        throw new MCException();
    }

    /** @deprecated */
    public ManagedCopy() {
        logger.unimplemented("*");
    }

    public void addMCEventListener(MCEventListener listener) {

    }

    public boolean cancelCopy() throws MCException {
        return false;
    }

    public void completeTransaction(
            String coupon, String majorMcotID, String minorMcotID,
            String mcotOfferInfo, String MCUi, String status,
            String MCOTParams) throws MCException {

    }

    public int getBDJKeepMode() throws MCException {
        return BDJKEEP_TERMINATE;
    }

    public byte[] getContentCertID() throws MCException {
        return null;
    }

    public String getContentID() throws MCException {
        return null;
    }

    public String getCoupon() throws MCException {
        return null;
    }

    public String getDealManifest() throws MCException {
        return null;
    }

    public URL getDefaultURL() throws MCException {
        throw new MCException();
    }

    public String getMajorMcotId() throws MCException {
        return null;
    }

    public String getMCMNonce() throws MCException {
        throw new MCException();
    }

    public MCOT[] getMCOTList() throws MCException {
        return new MCOT[0];
    }

    public String getMcotOfferInfo() throws MCException {
        return null;
    }

    public String getMCOTParams() throws MCException {
        return null;
    }

    public String getMCUi() throws MCException {
        return null;
    }

    public String getMinorMcotId() throws MCException {
        return null;
    }

    public String getOffer() throws MCException {
        return null;
    }

    public String getSessionId() throws MCException {
        return null;
    }

    public String getStatus() throws MCException {
        return null;
    }

    public void InvokeMCM() {

    }

    /** @deprecated */
    public boolean IsMCMSupported() {
        return false;
    }

    public MCProgress makeCopy() throws MCException {
        throw new MCException();
    }

    public void removeMCEventListener(MCEventListener listener) {

    }

    public String[] verifyOffers(String offers) throws MCException {
        return new String[0];
    }

    public boolean verifyPermission(String signature, String signedcontent)
            throws MCException {
        return false;
    }

    public static final int BDJKEEP_FULL = 1;
    public static final int BDJKEEP_LIMITED = 2;
    public static final int BDJKEEP_TERMINATE = 3;

    private static final Logger logger = Logger.getLogger(ManagedCopy.class.getName());
}
