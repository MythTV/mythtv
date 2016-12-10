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


package javax.tv.locator;

import org.bluray.net.BDLocator;
import org.videolan.Logger;

public class LocatorFactoryImpl extends LocatorFactory {
    public Locator createLocator(String url) throws MalformedLocatorException {
        if (url == null) {
            logger.error("null locator");
            throw new NullPointerException("Source Locator is null");
        }
        // check if it is a bluray locator
        if (url.startsWith("bd:/")) {
            try {
                return new BDLocator(url);
            } catch (org.davic.net.InvalidLocatorException ex) {
                logger.error("invalid locator: " + url);
                throw new MalformedLocatorException(ex.getMessage());
            }
        } else {
            // just return a generic locator
            logger.error("unknown locator type: " + url);
            return new LocatorImpl(url);
        }
    }

    public Locator[] transformLocator(Locator source) throws InvalidLocatorException {
        if (source == null) {
            logger.error("null locator");
            throw new NullPointerException("Source Locator is null");
        }
        if (source instanceof BDLocator){
            return new Locator[] { source };
        }
        logger.error("unsupported locator: " + source);
        throw new InvalidLocatorException(source, "Source locator is invalid.");
    }

    private static final Logger logger = Logger.getLogger(LocatorFactoryImpl.class.getName());
}
