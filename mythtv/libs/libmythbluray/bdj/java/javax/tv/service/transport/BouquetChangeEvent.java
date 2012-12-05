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

package javax.tv.service.transport;

import javax.tv.service.SIChangeType;

public class BouquetChangeEvent extends TransportSIChangeEvent {
    public BouquetChangeEvent(BouquetCollection collection, SIChangeType type,
            Bouquet bouquet)
    {
        super(collection, type, bouquet);
        this.collection = collection;
        this.bouquet = bouquet;
    }

    public BouquetCollection getBouquetCollection()
    {
        return collection;
    }

    public Bouquet getBouquet()
    {
        return bouquet;
    }

    private BouquetCollection collection;
    private Bouquet bouquet;
    private static final long serialVersionUID = 7452237638463210168L;
}
