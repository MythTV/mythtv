/*
 * This file is part of libbluray
 * Copyright (C) 2017  Petri Hintukainen <phintuka@users.sourceforge.net>
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
 *
 * Simple persistent storage for (key,value) pairs
 *
 */

#ifndef _BD_PROPERTIES_H_
#define _BD_PROPERTIES_H_

#include "util/attributes.h"

/*
 * property name: ASCII string, no '=' or '\n'
 * property value: UTF-8 string, no '\n'
 */

/**
 *
 *  Add / replace property value in file.
 *
 * @param file  full path to properties file
 * @param property  property name
 * @param val  value for property
 * @return 0 on success, -1 on error
 */
BD_PRIVATE int properties_put(const char *file, const char *property, const char *val);

/**
 *
 *  Read property value from file.
 *
 * @param file  full path to properties file
 * @param property  property name
 * @return property value or NULL
 */
BD_PRIVATE char *properties_get(const char *file, const char *property);


#endif /* _BD_PROPERTIES_H_ */
