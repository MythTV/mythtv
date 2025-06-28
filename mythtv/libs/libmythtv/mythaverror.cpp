/*
 *  Copyright (c) David Hampton 2020
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "mythaverror.h"

/** \brief A quick and dirty C++ equivalent to av_strerror

The caller must supply the stdstring.

\see av_strerror
*/
int av_strerror_stdstring (int errnum, std::string &errbuf)
{
    errbuf.resize(AV_ERROR_MAX_STRING_SIZE);
    int rc = av_strerror(errnum, errbuf.data(), errbuf.size());
    errbuf.resize(errbuf.find('\0'));
    return rc;
}

/** \brief A C++ equivalent to av_make_error_string

The caller must supply the stdstring so that the data pointer
remains valid after this function returns.

\see av_make_error_string
*/
char *av_make_error_stdstring(std::string &errbuf, int errnum)
{
    av_strerror_stdstring(errnum, errbuf);
    return errbuf.data();
}
