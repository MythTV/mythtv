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

#ifndef MYTHAVERROR_H
#define MYTHAVERROR_H

#include <string>
#include "mythexp.h"

extern "C" {
#include "libavutil/error.h"
}

MPUBLIC int av_strerror_stdstring (int errnum, std::string &errbuf);
MPUBLIC char *av_make_error_stdstring(std::string &errbuf, int errnum);
/**
A C++ equivalent to av_make_error_string() which does not need an input buffer,
similar to FFmpeg's av_err2str() macro.
*/
MPUBLIC inline std::string av_make_error_stdstring(int errnum)
{
    auto errbuf = std::string(AV_ERROR_MAX_STRING_SIZE, '\0'); // must use () to select correct constructor
    av_strerror(errnum, errbuf.data(), errbuf.size());
    errbuf.resize(errbuf.find('\0'));
    return errbuf;
}
/**
@return the string returned by av_str_error() if it succeeded, otherwise returns
        "UNKNOWN" instead of av_str_error()'s generic message which indicates
        the input errnum.
*/
MPUBLIC inline std::string av_make_error_stdstring_unknown(int errnum)
{
    using namespace std::string_literals;
    auto errbuf = std::string(AV_ERROR_MAX_STRING_SIZE, '\0'); // must use () to select correct constructor
    int rc = av_strerror(errnum, errbuf.data(), errbuf.size());
    if (rc == 0)
    {
        errbuf.resize(errbuf.find('\0'));
        return errbuf;
    }
    return "UNKNOWN"s;
}

#endif // MYTHAVERROR_H
