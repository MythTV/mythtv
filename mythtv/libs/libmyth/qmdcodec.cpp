/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   RFC 1321 "MD5 Message-Digest Algorithm" Copyright (C) 1991-1992.
   RSA Data Security, Inc. Created 1991. All rights reserved.

   The QMD5 class is based on a C++ implementation of
   "RSA Data Security, Inc. MD5 Message-Digest Algorithm" by
   Mordechai T. Abzug,	Copyright (c) 1995.  This implementation
   passes the test-suite as defined in RFC 1321.

   The encoding and decoding utilities in QCodecs with the exception of
   quoted-printable are based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.

   The quoted-printable codec as described in RFC 2045, section 6.7. is by
   Rik Hemsley (C) 2001.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "qmdcodec.h"

#define QMD5_S11 7
#define QMD5_S12 12
#define QMD5_S13 17
#define QMD5_S14 22
#define QMD5_S21 5
#define QMD5_S22 9
#define QMD5_S23 14
#define QMD5_S24 20
#define QMD5_S31 4
#define QMD5_S32 11
#define QMD5_S33 16
#define QMD5_S34 23
#define QMD5_S41 6
#define QMD5_S42 10
#define QMD5_S43 15
#define QMD5_S44 21

const char QCodecs::Base64EncMap[64] =
{
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
  0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
  0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
  0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
  0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F
};

const char QCodecs::Base64DecMap[128] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x3F,
  0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
  0x3C, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
  0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
  0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
  0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char QCodecs::UUEncMap[64] =
{
  0x60, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F
};

const char QCodecs::UUDecMap[128] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char QCodecs::hexChars[16] =
{
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

const unsigned int QCodecs::maxQPLineLength = 70;


/******************************** QCodecs ********************************/
// strchr(3) for broken systems.
static int rikFindChar(register const char * _s, const char c)
{
  register const char * s = _s;

  while (true)
  {
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
  }

  return s - _s;
}

QCString QCodecs::quotedPrintableEncode(const QByteArray& in, bool useCRLF)
{
  QByteArray out;
  quotedPrintableEncode (in, out, useCRLF);
  return QCString (out.data(), out.size()+1);
}

QCString QCodecs::quotedPrintableEncode(const QCString& str, bool useCRLF)
{
  if (str.isEmpty())
    return "";

  QByteArray in (str.length());
  memcpy (in.data(), str.data(), str.length());
  return quotedPrintableEncode(in, useCRLF);
}

void QCodecs::quotedPrintableEncode(const QByteArray& in, QByteArray& out, bool useCRLF)
{
  out.resize (0);
  if (in.isEmpty())
    return;

  char *cursor;
  const char *data;
  unsigned int lineLength;
  unsigned int pos;

  const unsigned int length = in.size();
  const unsigned int end = length - 1;


  // Reasonable guess for output size when we're encoding
  // mostly-ASCII data. It doesn't really matter, because
  // the underlying allocation routines are quite efficient,
  // but it's nice to have 0 allocations in many cases.
  out.resize ((length*12)/10);
  cursor = out.data();
  data = in.data();
  lineLength = 0;
  pos = 0;

  for (unsigned int i = 0; i < length; i++)
  {
    unsigned char c (data[i]);

    // check if we have to enlarge the output buffer, use
    // a safety margin of 16 byte
    pos = cursor-out.data();
    if (out.size()-pos < 16) {
      out.resize(out.size()+4096);
      cursor = out.data()+pos;
    }

    // Plain ASCII chars just go straight out.

    if ((c >= 33) && (c <= 126) && ('=' != c))
    {
      *cursor++ = c;
      ++lineLength;
    }

    // Spaces need some thought. We have to encode them at eol (or eof).

    else if (' ' == c)
    {
      if
        (
         (i >= length)
         ||
         ((i < end) && ((useCRLF && ('\r' == data[i + 1]) && ('\n' == data[i + 2]))
                        ||
                        (!useCRLF && ('\n' == data[i + 1]))))
        )
      {
        *cursor++ = '=';
        *cursor++ = '2';
        *cursor++ = '0';

        lineLength += 3;
      }
      else
      {
        *cursor++ = ' ';
        ++lineLength;
      }
    }
    // If we find a line break, just let it through.
    else if ((useCRLF && ('\r' == c) && (i < end) && ('\n' == data[i + 1])) ||
             (!useCRLF && ('\n' == c)))
    {
      lineLength = 0;

      if (useCRLF) {
        *cursor++ = '\r';
        *cursor++ = '\n';
        ++i;
      } else {
        *cursor++ = '\n';
      }
    }

    // Anything else is converted to =XX.

    else
    {
      *cursor++ = '=';
      *cursor++ = hexChars[c / 16];
      *cursor++ = hexChars[c % 16];

      lineLength += 3;
    }

    // If we're approaching the maximum line length, do a soft line break.

    if ((lineLength > maxQPLineLength) && (i < end))
    {
      if (useCRLF) {
        *cursor++ = '=';
        *cursor++ = '\r';
        *cursor++ = '\n';
      } else {
        *cursor++ = '=';
        *cursor++ = '\n';
      }

      lineLength = 0;
    }
  }

  out.truncate(cursor - out.data());
}

QCString QCodecs::quotedPrintableDecode(const QByteArray & in)
{
  QByteArray out;
  quotedPrintableDecode (in, out);
  return QCString (out.data(), out.size()+1);
}

QCString QCodecs::quotedPrintableDecode(const QCString & str)
{
  if (str.isEmpty())
    return "";

  QByteArray in (str.length());
  memcpy (in.data(), str.data(), str.length());
  return quotedPrintableDecode (in);
}

void QCodecs::quotedPrintableDecode(const QByteArray& in, QByteArray& out)
{
  // clear out the output buffer
  out.resize (0);
  if (in.isEmpty())
      return;

  char *cursor;
  const char *data;
  const unsigned int length = in.size();

  data = in.data();
  out.resize (length);
  cursor = out.data();

  for (unsigned int i = 0; i < length; i++)
  {
    char c(in.at(i));

    if ('=' == c)
    {
      if (i < length - 2)
      {
        char c1 = in.at(i + 1);
        char c2 = in.at(i + 2);

        if (('\n' == c1) || ('\r' == c1 && '\n' == c2))
        {
          // Soft line break. No output.
          if ('\r' == c1)
            i += 2;        // CRLF line breaks
          else
            i += 1;
        }
        else
        {
          // =XX encoded byte.

          int hexChar0 = rikFindChar(hexChars, c1);
          int hexChar1 = rikFindChar(hexChars, c2);

          if (hexChar0 < 16 && hexChar1 < 16)
          {
            *cursor++ = char((hexChar0 * 16) | hexChar1);
            i += 2;
          }
        }
      }
    }
    else
    {
      *cursor++ = c;
    }
  }

  out.truncate(cursor - out.data());
}

QCString QCodecs::base64Encode( const QCString& str, bool insertLFs )
{
    if ( str.isEmpty() )
        return "";

    QByteArray in (str.length());
    memcpy( in.data(), str.data(), str.length() );
    return base64Encode( in, insertLFs );
}

QCString QCodecs::base64Encode( const QByteArray& in, bool insertLFs )
{
    QByteArray out;
    base64Encode( in, out, insertLFs );
    return QCString( out.data(), out.size()+1 );
}

void QCodecs::base64Encode( const QByteArray& in, QByteArray& out,
                            bool insertLFs )
{
    // clear out the output buffer
    out.resize (0);
    if ( in.isEmpty() )
        return;

    unsigned int sidx = 0;
    unsigned int didx = 0;
    const char* data = in.data();
    const unsigned int len = in.size();

    unsigned int out_len = ((len+2)/3)*4;

    // Deal with the 76 characters or less per
    // line limit specified in RFC 2045 on a
    // pre request basis.
    insertLFs = (insertLFs && out_len > 76);
    if ( insertLFs )
      out_len += ((out_len-1)/76);

    int count = 0;
    out.resize( out_len );

    // 3-byte to 4-byte conversion + 0-63 to ascii printable conversion
    if ( len > 1 )
    {
        while (sidx < len-2)
        {
            if ( insertLFs )
            {
                if ( count && (count%76) == 0 )
                    out.data()[didx++] = '\n';
                count += 4;
            }
            out.data()[didx++] = Base64EncMap[(data[sidx] >> 2) & 077];
            out.data()[didx++] = Base64EncMap[(data[sidx+1] >> 4) & 017 |
                                       (data[sidx] << 4) & 077];
            out.data()[didx++] = Base64EncMap[(data[sidx+2] >> 6) & 003 |
                                       (data[sidx+1] << 2) & 077];
            out.data()[didx++] = Base64EncMap[data[sidx+2] & 077];
            sidx += 3;
        }
    }

    if (sidx < len)
    {
        if ( insertLFs && (count > 0) && (count%76) == 0 )
           out.data()[didx++] = '\n';

        out.data()[didx++] = Base64EncMap[(data[sidx] >> 2) & 077];
        if (sidx < len-1)
        {
            out.data()[didx++] = Base64EncMap[(data[sidx+1] >> 4) & 017 |
												(data[sidx] << 4) & 077];
            out.data()[didx++] = Base64EncMap[(data[sidx+1] << 2) & 077];
        }
        else
        {
            out.data()[didx++] = Base64EncMap[(data[sidx] << 4) & 077];
        }
    }

    // Add padding
    while (didx < out.size())
    {
        out.data()[didx] = '=';
        didx++;
    }
}

QCString QCodecs::base64Decode( const QCString& str )
{
    if ( str.isEmpty() )
        return "";

    QByteArray in( str.length() );
    memcpy( in.data(), str.data(), str.length() );
    return base64Decode( in );
}

QCString QCodecs::base64Decode( const QByteArray& in )
{
    QByteArray out;
    base64Decode( in, out );
    return QCString( out.data(), out.size()+1 );
}

void QCodecs::base64Decode( const QByteArray& in, QByteArray& out )
{
    out.resize(0);
    if ( in.isEmpty() )
        return;

    unsigned int count = 0;
    unsigned int len = in.size(), tail = len;
    const char* data = in.data();

    // Deal with possible *nix "BEGIN" marker!!
    while ( count < len && (data[count] == '\n' || data[count] == '\r' ||
            data[count] == '\t' || data[count] == ' ') )
        count++;

#ifdef _WIN32
    if ( strnicmp(data+count, "begin", 5) == 0 )
#else
	if ( strncasecmp(data+count, "begin", 5) == 0 )
#endif
    {
        count += 5;
        while ( count < len && data[count] != '\n' && data[count] != '\r' )
            count++;

        while ( count < len && (data[count] == '\n' || data[count] == '\r') )
            count ++;

        data += count;
        tail = (len -= count);
    }

    // Find the tail end of the actual encoded data even if
    // there is/are trailing CR and/or LF.
    while ( data[tail-1] == '=' || data[tail-1] == '\n' ||
            data[tail-1] == '\r' )
        if ( data[--tail] != '=' ) len = tail;

    unsigned int outIdx = 0;
    out.resize( (count=len) );
    for (unsigned int idx = 0; idx < count; idx++)
    {
        // Adhere to RFC 2045 and ignore characters
        // that are not part of the encoding table.
        unsigned char ch = data[idx];
        if ((ch > 47 && ch < 58) || (ch > 64 && ch < 91) ||
            (ch > 96 && ch < 123) || ch == '+' || ch == '/' || ch == '=')
        {
            out.data()[outIdx++] = Base64DecMap[ch];
        }
        else
        {
            len--;
            tail--;
        }
    }

    // kdDebug() << "Tail size = " << tail << ", Length size = " << len << endl;

    // 4-byte to 3-byte conversion
    len = (tail>(len/4)) ? tail-(len/4) : 0;
    unsigned int sidx = 0, didx = 0;
    if ( len > 1 )
    {
      while (didx < len-2)
      {
          out.data()[didx] = (((out.at(sidx) << 2) & 255) | ((out.at(sidx+1) >> 4) & 003));
          out.data()[didx+1] = (((out.at(sidx+1) << 4) & 255) | ((out.at(sidx+2) >> 2) & 017));
          out.data()[didx+2] = (((out.at(sidx+2) << 6) & 255) | (out.at(sidx+3) & 077));
          sidx += 4;
          didx += 3;
      }
    }

    if (didx < len)
        out.data()[didx] = (((out.at(sidx) << 2) & 255) | ((out.at(sidx+1) >> 4) & 003));

    if (++didx < len )
        out.data()[didx] = (((out.at(sidx+1) << 4) & 255) | ((out.at(sidx+2) >> 2) & 017));

    // Resize the output buffer
    if ( len == 0 || len < out.size() )
      out.resize(len);
}

QCString QCodecs::uuencode( const QCString& str )
{
    if ( str.isEmpty() )
        return "";

    QByteArray in;
    in.resize( str.length() );
    memcpy( in.data(), str.data(), str.length() );
    return uuencode( in );
}

QCString QCodecs::uuencode( const QByteArray& in )
{
    QByteArray out;
    uuencode( in, out );
    return QCString( out.data(), out.size()+1 );
}

void QCodecs::uuencode( const QByteArray& in, QByteArray& out )
{
    out.resize( 0 );
    if( in.isEmpty() )
        return;

    unsigned int sidx = 0;
    unsigned int didx = 0;
    unsigned int line_len = 45;

    const char nl[] = "\n";
    const char* data = in.data();
    const unsigned int nl_len = strlen(nl);
    const unsigned int len = in.size();

    out.resize( (len+2)/3*4 + ((len+line_len-1)/line_len)*(nl_len+1) );
    // split into lines, adding line-length and line terminator
    while (sidx+line_len < len)
    {
        // line length
        out.data()[didx++] = UUEncMap[line_len];

        // 3-byte to 4-byte conversion + 0-63 to ascii printable conversion
        for (unsigned int end = sidx+line_len; sidx < end; sidx += 3)
        {
            out.data()[didx++] = UUEncMap[(data[sidx] >> 2) & 077];
            out.data()[didx++] = UUEncMap[(data[sidx+1] >> 4) & 017 |
                                   (data[sidx] << 4) & 077];
            out.data()[didx++] = UUEncMap[(data[sidx+2] >> 6) & 003 |
                                (data[sidx+1] << 2) & 077];
            out.data()[didx++] = UUEncMap[data[sidx+2] & 077];
        }

        // line terminator
        //for (unsigned int idx=0; idx < nl_len; idx++)
        //out[didx++] = nl[idx];
        memcpy(out.data()+didx, nl, nl_len);
        didx += nl_len;
    }

    // line length
    out.data()[didx++] = UUEncMap[len-sidx];
    // 3-byte to 4-byte conversion + 0-63 to ascii printable conversion
    while (sidx+2 < len)
    {
        out.data()[didx++] = UUEncMap[(data[sidx] >> 2) & 077];
        out.data()[didx++] = UUEncMap[(data[sidx+1] >> 4) & 017 |
                               (data[sidx] << 4) & 077];
        out.data()[didx++] = UUEncMap[(data[sidx+2] >> 6) & 003 |
                               (data[sidx+1] << 2) & 077];
        out.data()[didx++] = UUEncMap[data[sidx+2] & 077];
        sidx += 3;
    }

    if (sidx < len-1)
    {
        out.data()[didx++] = UUEncMap[(data[sidx] >> 2) & 077];
        out.data()[didx++] = UUEncMap[(data[sidx+1] >> 4) & 017 |
                               (data[sidx] << 4) & 077];
        out.data()[didx++] = UUEncMap[(data[sidx+1] << 2) & 077];
        out.data()[didx++] = UUEncMap[0];
    }
    else if (sidx < len)
    {
        out.data()[didx++] = UUEncMap[(data[sidx] >> 2) & 077];
        out.data()[didx++] = UUEncMap[(data[sidx] << 4) & 077];
        out.data()[didx++] = UUEncMap[0];
        out.data()[didx++] = UUEncMap[0];
    }

    // line terminator
    memcpy(out.data()+didx, nl, nl_len);
    didx += nl_len;

    // sanity check
    if ( didx != out.size() )
        out.resize( 0 );
}

QCString QCodecs::uudecode( const QCString& str )
{
    if ( str.isEmpty() )
        return "";

    QByteArray in;
    in.resize( str.length() );
    memcpy( in.data(), str.data(), str.length() );
    return uudecode( in );
}

QCString QCodecs::uudecode( const QByteArray& in )
{
    QByteArray out;
    uudecode( in, out );
    return QCString( out.data(), out.size()+1 );
}

void QCodecs::uudecode( const QByteArray& in, QByteArray& out )
{
    out.resize( 0 );
    if( in.isEmpty() )
        return;

    unsigned int sidx = 0;
    unsigned int didx = 0;
    unsigned int len = in.size();
    unsigned int line_len, end;
    const char* data = in.data();

    // Deal with *nix "BEGIN"/"END" separators!!
    unsigned int count = 0;
    while ( count < len && (data[count] == '\n' || data[count] == '\r' ||
            data[count] == '\t' || data[count] == ' ') )
        count ++;

    bool hasLF = false;

#ifdef _WIN32
    if ( strnicmp( data+count, "begin", 5) == 0 )
#else
	if ( strncasecmp( data+count, "begin", 5) == 0 )
#endif
    {
        count += 5;
        while ( count < len && data[count] != '\n' && data[count] != '\r' )
            count ++;

        while ( count < len && (data[count] == '\n' || data[count] == '\r') )
            count ++;

        data += count;
        len -= count;
        hasLF = true;
    }

    out.resize( len/4*3 );
    while ( sidx < len )
    {
        // get line length (in number of encoded octets)
        line_len = UUDecMap[ (unsigned char) data[sidx++]];
        // ascii printable to 0-63 and 4-byte to 3-byte conversion
        end = didx+line_len;
        char A, B, C, D;
        if (end > 2) {
          while (didx < end-2)
          {
             A = UUDecMap[(unsigned char) data[sidx]];
             B = UUDecMap[(unsigned char) data[sidx+1]];
             C = UUDecMap[(unsigned char) data[sidx+2]];
             D = UUDecMap[(unsigned char) data[sidx+3]];
             out.data()[didx++] = ( ((A << 2) & 255) | ((B >> 4) & 003) );
             out.data()[didx++] = ( ((B << 4) & 255) | ((C >> 2) & 017) );
             out.data()[didx++] = ( ((C << 6) & 255) | (D & 077) );
             sidx += 4;
          }
        }

        if (didx < end)
        {
            A = UUDecMap[(unsigned char) data[sidx]];
            B = UUDecMap[(unsigned char) data[sidx+1]];
            out.data()[didx++] = ( ((A << 2) & 255) | ((B >> 4) & 003) );
        }

        if (didx < end)
        {
            B = UUDecMap[(unsigned char) data[sidx+1]];
            C = UUDecMap[(unsigned char) data[sidx+2]];
            out.data()[didx++] = ( ((B << 4) & 255) | ((C >> 2) & 017) );
        }

        // skip padding
        while (sidx < len  && data[sidx] != '\n' && data[sidx] != '\r')
            sidx++;

        // skip end of line
        while (sidx < len  && (data[sidx] == '\n' || data[sidx] == '\r'))
            sidx++;

        // skip the "END" separator when present.
#ifdef _WIN32
        if ( hasLF && strnicmp( data+sidx, "end", 3) == 0 )
#else
		if ( hasLF && strncasecmp( data+sidx, "end", 3) == 0 )
#endif
            break;
    }

    if ( didx < out.size()  )
        out.resize( didx );
}

/******************************** QMD5 ********************************/
QMD5::QMD5()
{
    init();
}

QMD5::QMD5(const char *in, int len)
{
    init();
    update(in, len);
}

QMD5::QMD5(const QByteArray& in)
{
    init();
    update( in );
}

QMD5::QMD5(const QCString& in)
{
    init();
    update( in );
}

void QMD5::update(const QByteArray& in)
{
    update(in.data(), int(in.size()));
}

void QMD5::update(const QCString& in)
{
    update(in.data(), int(in.length()));
}

void QMD5::update(const unsigned char* in, int len)
{
    if (len < 0)
        len = qstrlen(reinterpret_cast<const char*>(in));

    if (!len)
        return;

    if (m_finalized) {
        qWarning("QMD5::update called after state was finalized!");
        return;
    }

    Q_UINT32 in_index;
    Q_UINT32 buffer_index;
    Q_UINT32 buffer_space;
    Q_UINT32 in_length = static_cast<Q_UINT32>( len );

    buffer_index = static_cast<Q_UINT32>((m_count[0] >> 3) & 0x3F);

    if (  (m_count[0] += (in_length << 3))<(in_length << 3) )
        m_count[1]++;

    m_count[1] += (in_length >> 29);
    buffer_space = 64 - buffer_index;

    if (in_length >= buffer_space)
    {
        memcpy (m_buffer + buffer_index, in, buffer_space);
        transform (m_buffer);

        for (in_index = buffer_space; in_index + 63 < in_length;
             in_index += 64)
            transform (reinterpret_cast<const unsigned char*>(in+in_index));

        buffer_index = 0;
    }
    else
        in_index=0;

    memcpy(m_buffer+buffer_index, in+in_index, in_length-in_index);
}

bool QMD5::update(QIODevice& file)
{
    char buffer[1024];
    int len;

    while ((len=file.readBlock(reinterpret_cast<char*>(buffer), sizeof(buffer))) > 0)
        update(buffer, len);

    return file.atEnd();
}

void QMD5::finalize ()
{
    if (m_finalized) return;

    Q_UINT8 bits[8];
    Q_UINT32 index, padLen;
    static unsigned char PADDING[64]=
    {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    encode (bits, m_count, 8);
    //memcpy( bits, m_count, 8 );

    // Pad out to 56 mod 64.
    index = static_cast<Q_UINT32>((m_count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update (reinterpret_cast<const char*>(PADDING), padLen);

    // Append length (before padding)
    update (reinterpret_cast<const char*>(bits), 8);

    // Store state in digest
    encode (m_digest, m_state, 16);
    //memcpy( m_digest, m_state, 16 );

    // Fill sensitive information with zero's
    memset ( (void *)m_buffer, 0, sizeof(*m_buffer));

    m_finalized = true;
}


bool QMD5::verify( const QMD5::Digest& digest)
{
    finalize();
    return (0 == memcmp(rawDigest(), digest, sizeof(QMD5::Digest)));
}

bool QMD5::verify( const QCString& hexdigest)
{
    finalize();
    return (0 == strcmp(hexDigest().data(), hexdigest));
}

const QMD5::Digest& QMD5::rawDigest()
{
    finalize();
    return m_digest;
}

void QMD5::rawDigest( QMD5::Digest& bin )
{
    finalize();
    memcpy( bin, m_digest, 16 );
}


QCString QMD5::hexDigest()
{
    QCString s(33);

    finalize();
    sprintf(s.data(), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            m_digest[0], m_digest[1], m_digest[2], m_digest[3], m_digest[4], m_digest[5],
            m_digest[6], m_digest[7], m_digest[8], m_digest[9], m_digest[10], m_digest[11],
            m_digest[12], m_digest[13], m_digest[14], m_digest[15]);

    return s;
}

void QMD5::hexDigest(QCString& s)
{
    finalize();
    s.resize(33);
    sprintf(s.data(), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            m_digest[0], m_digest[1], m_digest[2], m_digest[3], m_digest[4], m_digest[5],
            m_digest[6], m_digest[7], m_digest[8], m_digest[9], m_digest[10], m_digest[11],
            m_digest[12], m_digest[13], m_digest[14], m_digest[15]);
}

QCString QMD5::base64Digest()
{
    QByteArray ba(16);

    finalize();
    memcpy(ba.data(), m_digest, 16);
    return QCodecs::base64Encode(ba);
}


void QMD5::init()
{
    d = 0;
    reset();
}

void QMD5::reset()
{
    m_finalized = false;

    m_count[0] = 0;
    m_count[1] = 0;

    m_state[0] = 0x67452301;
    m_state[1] = 0xefcdab89;
    m_state[2] = 0x98badcfe;
    m_state[3] = 0x10325476;

    memset ( m_buffer, 0, sizeof(*m_buffer));
    memset ( m_digest, 0, sizeof(*m_digest));
}

void QMD5::transform( const unsigned char block[64] )
{

    Q_UINT32 a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], x[16];

    decode (x, block, 64);
    //memcpy( x, block, 64 );

    Q_ASSERT(!m_finalized);  // not just a user error, since the method is private

    /* Round 1 */
    FF (a, b, c, d, x[ 0], QMD5_S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], QMD5_S12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], QMD5_S13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], QMD5_S14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], QMD5_S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], QMD5_S12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], QMD5_S13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], QMD5_S14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], QMD5_S11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], QMD5_S12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], QMD5_S13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], QMD5_S14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], QMD5_S11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], QMD5_S12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], QMD5_S13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], QMD5_S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG (a, b, c, d, x[ 1], QMD5_S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], QMD5_S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], QMD5_S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], QMD5_S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], QMD5_S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], QMD5_S22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], QMD5_S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], QMD5_S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], QMD5_S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], QMD5_S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], QMD5_S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], QMD5_S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], QMD5_S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], QMD5_S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], QMD5_S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], QMD5_S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, x[ 5], QMD5_S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], QMD5_S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], QMD5_S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], QMD5_S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], QMD5_S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], QMD5_S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], QMD5_S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], QMD5_S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], QMD5_S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], QMD5_S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], QMD5_S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], QMD5_S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], QMD5_S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], QMD5_S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], QMD5_S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], QMD5_S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, x[ 0], QMD5_S41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], QMD5_S42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], QMD5_S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], QMD5_S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], QMD5_S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], QMD5_S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], QMD5_S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], QMD5_S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], QMD5_S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], QMD5_S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], QMD5_S43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], QMD5_S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], QMD5_S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], QMD5_S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], QMD5_S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], QMD5_S44, 0xeb86d391); /* 64 */

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;

    memset ( static_cast<void *>(x), 0, sizeof(x) );
}

inline Q_UINT32 QMD5::rotate_left (Q_UINT32 x, Q_UINT32 n)
{
    return (x << n) | (x >> (32-n))  ;
}

inline Q_UINT32 QMD5::F (Q_UINT32 x, Q_UINT32 y, Q_UINT32 z)
{
    return (x & y) | (~x & z);
}

inline Q_UINT32 QMD5::G (Q_UINT32 x, Q_UINT32 y, Q_UINT32 z)
{
    return (x & z) | (y & ~z);
}

inline Q_UINT32 QMD5::H (Q_UINT32 x, Q_UINT32 y, Q_UINT32 z)
{
    return x ^ y ^ z;
}

inline Q_UINT32 QMD5::I (Q_UINT32 x, Q_UINT32 y, Q_UINT32 z)
{
    return y ^ (x | ~z);
}

void QMD5::FF ( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d,
                       Q_UINT32 x, Q_UINT32  s, Q_UINT32 ac )
{
    a += F(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

void QMD5::GG ( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d,
                 Q_UINT32 x, Q_UINT32 s, Q_UINT32 ac)
{
    a += G(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

void QMD5::HH ( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d,
                 Q_UINT32 x, Q_UINT32 s, Q_UINT32 ac )
{
    a += H(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

void QMD5::II ( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d,
                 Q_UINT32 x, Q_UINT32 s, Q_UINT32 ac )
{
    a += I(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}


void QMD5::encode ( unsigned char* output, Q_UINT32 *in, Q_UINT32 len )
{
#if !defined(WORDS_BIGENDIAN)
    memcpy(output, in, len);

#else
    Q_UINT32 i, j;
    for (i = 0, j = 0; j < len; i++, j += 4)
    {
        output[j]   = static_cast<Q_UINT8>((in[i] & 0xff));
        output[j+1] = static_cast<Q_UINT8>(((in[i] >> 8) & 0xff));
        output[j+2] = static_cast<Q_UINT8>(((in[i] >> 16) & 0xff));
        output[j+3] = static_cast<Q_UINT8>(((in[i] >> 24) & 0xff));
    }
#endif
}

// Decodes in (Q_UINT8) into output (Q_UINT32). Assumes len is a
// multiple of 4.
void QMD5::decode (Q_UINT32 *output, const unsigned char* in, Q_UINT32 len)
{
#if !defined(WORDS_BIGENDIAN)
    memcpy(output, in, len);

#else
    Q_UINT32 i, j;
    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = static_cast<Q_UINT32>(in[j]) |
                    (static_cast<Q_UINT32>(in[j+1]) << 8)  |
                    (static_cast<Q_UINT32>(in[j+2]) << 16) |
                    (static_cast<Q_UINT32>(in[j+3]) << 24);
#endif
}
