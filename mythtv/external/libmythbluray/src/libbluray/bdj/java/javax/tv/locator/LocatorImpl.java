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

public class LocatorImpl implements Locator {

    public LocatorImpl(String url) {
        this.url = url;
    }

    public boolean hasMultipleTransformations() {
        return false;
    }

    public String toExternalForm() {
        return url;
    }

    public String toString() {
        return toExternalForm();
    }

    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((url == null) ? 0 : url.hashCode());
        return result;
    }

    public boolean equals(Object obj) {
        if (!(obj instanceof Locator))
            return false;

        Locator other = (Locator) obj;
        String extForm = toExternalForm();
        if (extForm == null) {
            return other.toExternalForm() == null;
        }
        return extForm.equals(other.toExternalForm());
    }

    private String url;
}
