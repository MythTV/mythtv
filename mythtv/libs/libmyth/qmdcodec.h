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

   The KMD5 class is based on a C++ implementation of
   "RSA Data Security, Inc. MD5 Message-Digest Algorithm" by
   Mordechai T. Abzug,	Copyright (c) 1995.  This implementation
   passes the test-suite as defined in RFC 1321.

   The encoding and decoding utilities in KCodecs with the exception of
   quoted-printable are based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.

   The quoted-printable codec as described in RFC 2045, section 6.7. is by
   Rik Hemsley (C) 2001.
*/

#ifndef _QMDCODEC_H
#define _QMDCODEC_H

#define QBase64 QCodecs

#include <qglobal.h>
#include <qstring.h>
#include <qiodevice.h>

/**
 * A wrapper class for the most commonly used encoding and
 * decoding algorithms.  Currently there is support for encoding
 * and decoding input using base64, uu and the quoted-printable
 * specifications.
 *
 * @sect Usage:
 *
 * <PRE>
 * QCString input = "Aladdin:open sesame";
 * QCString result = QCodecs::base64Encode(input);
 * cout << "Result: " << result.data() << endl;
 *
 * Output should be
 * Result: QWxhZGRpbjpvcGVuIHNlc2FtZQ==
 * </PRE>
 *
 * The above example makes use of the convenience functions
 * (ones that accept/return null-terminated strings) to encode/decode
 * a string.  If what you need is to encode or decode binary data, then
 * it is highly recommended that you use the functions that take an input
 * and output QByteArray as arguments.  These functions are specifically
 * tailored for encoding and decoding binary data.
 *
 * @short A collection of commonly used encoding and decoding algorithms.
 * @author Dawit Alemayehu <adawit@kde.org>
 * @author Rik Hemsley <rik@kde.org>
 */
class QCodecs
{
public:

  /**
   * Encodes the given data using the quoted-printable algorithm.
   *
   * @param in      data to be encoded.
   * @param useCRLF if true the input data is expected to have
   *                CRLF line breaks and the output will have CRLF line
   *                breaks, too.
   * @return        quoted-printable encoded data.
   */
  static QCString quotedPrintableEncode(const QByteArray & in,
                                        bool useCRLF = true);

  /**
   * @overload
   *
   * Same as above except it accepts a null terminated
   * string instead an array.
   *
   * @param str     data to be encoded.
   * @param useCRLF if true the input data is expected to have
   *                CRLF line breaks and the output will have CRLF line
   *                breaks, too.
   * @return        quoted-printable encoded data.
   */
  static QCString quotedPrintableEncode(const QCString & str,
                                        bool useCRLF = true);

  /**
   * Encodes the given data using the quoted-printable algorithm.
   *
   * Use this function if you want the result of the encoding
   * to be placed in another array which cuts down the number
   * of copy operation that have to be performed in the process.
   * This is also the preferred method for encoding binary data.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in      data to be encoded.
   * @param out     decoded data.
   * @param useCRLF if true the input data is expected to have
   *                CRLF line breaks and the output will have CRLF line
   *                breaks, too.
   * @return        quoted-printable encoded data.
   */
  static void quotedPrintableEncode(const QByteArray & in, QByteArray& out,
                                    bool useCRLF);

  /**
   * Decodes a quoted-printable encoded string.
   *
   * Accepts data with CRLF or standard unix line breaks.
   *
   * @param in  the data to be decoded.
   * @return    decoded data.
   */
  static QCString quotedPrintableDecode(const QByteArray & in);

  /**
   * @overload
   *
   * Same as above except it accepts a null terminated
   * string instead an array.
   *
   * @param str  the data to be decoded.
   * @return     decoded data.
   */
  static QCString quotedPrintableDecode(const QCString & str);

  /**
   * Decodes a quoted-printable encoded data.
   *
   * Accepts data with CRLF or standard unix line breaks.
   * Use this function if you want the result of the decoding
   * to be placed in another array which cuts down the number
   * of copy operation that have to be performed in the process.
   * This is also the preferred method for decoding an encoded
   * binary data.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in   data to be encoded.
   * @param out  decoded data.
   *
   * @return     quoted-printable encoded data.
   */
  static void quotedPrintableDecode(const QByteArray & in, QByteArray& out);


  /**
   * Encodes the given data using the uuencode algorithm.
   *
   * The output is split into lines starting with the number of
   * encoded octets in the line and ending with a newline.  No
   * line is longer than 45 octets (60 characters), excluding the
   * line terminator.
   *
   * @param in   the data to be uuencoded
   * @return     a uuencoded data.
   */
  static QCString uuencode( const QByteArray& in );

  /**
   * @overload
   *
   * Same as the above functions except it accepts
   * a null terminated string instead an array.
   *
   * @param str   the string to be uuencoded.
   * @return      the encoded string.
   */
  static QCString uuencode( const QCString& str );

  /**
   * Encodes the given data using the uuencode algorithm.
   *
   * Use this function if you want the result of the encoding
   * to be placed in another array and cut down the number of
   * copy operation that have to be performed in the process.
   * This is the preffered method for encoding binary data.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in   the data to be uuencoded.
   * @param out  the container for the uudecoded data.
   */
  static void uuencode( const QByteArray& in, QByteArray& out );

  /**
   * Decodes the given data using the uuencode algorithm.
   *
   * Any 'begin' and 'end' lines like those generated by
   * the utilities in unix and unix-like OS will be
   * automatically ignored.
   *
   * @param in   the data uuencoded data to be decoded.
   * @return     a decoded string.
   */
  static QCString uudecode( const QByteArray& in );

  /**
   * @overload
   *
   * Same as the above functions except it accepts
   * a null terminated string instead an array.
   *
   * @param str   the string to be decoded.
   * @return      a uudecoded string.
   */
  static QCString uudecode( const QCString& str );

  /**
   * Decodes the given data using the uudecode algorithm.
   *
   * Use this function if you want the result of the decoding
   * to be placed in another array which cuts down the number
   * of copy operation that have to be performed in the process.
   * This is the preferred method for decoding binary data.
   *
   * Any 'begin' and 'end' lines like those generated by
   * the utilities in unix and unix-like OS will be
   * automatically ignored.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in   the uuencoded-data to be decoded.
   * @param out  the container for the uudecoded data.
   */
  static void uudecode( const QByteArray& in, QByteArray& out );


  /**
   * Encodes the given data using the base64 algorithm.
   *
   * The boolean argument determines if the encoded data is
   * going to be restricted to 76 characters or less per line
   * as specified by RFC 2045.  If @p insertLFs is true, then
   * there will be 76 characters or less per line.
   *
   * @param in         the data to be encoded.
   * @param insertLFs  limit the number of characters per line.
   *
   * @return           a base64 encoded string.
   */
  static QCString base64Encode( const QByteArray& in, bool insertLFs = false);

  /**
   * @overload
   *
   * Same as the above functions except it accepts
   * a null terminated string instead an array.
   *
   * @param str       the string to be encoded.
   * @param insertLFs limit the number of characters per line.
   * @return          the decoded string.
   */
  static QCString base64Encode( const QCString& str, bool insertLFs = false );

  /**
   * Encodes the given data using the base64 algorithm.
   *
   * Use this function if you want the result of the encoding
   * to be placed in another array which cuts down the number
   * of copy operation that have to be performed in the process.
   * This is also the preferred method for encoding binary data.
   *
   * The boolean argument determines if the encoded data is going
   * to be restricted to 76 characters or less per line as specified
   * by RFC 2045.  If @p insertLFs is true, then there will be 76
   * characters or less per line.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in        the data to be encoded using base64.
   * @param out       the container for the encoded data.
   * @param insertLFs limit the number of characters per line.
   */
  static void base64Encode( const QByteArray& in, QByteArray& out,
                            bool insertLFs = false );

  /**
   * Decodes the given data that was encoded using the
   * base64 algorithm.
   *
   * @param in   the base64-encoded data to be decoded.
   * @return     the decoded data.
   */
  static QCString base64Decode( const QByteArray& in );

  /**
   * @overload
   *
   * Same as the above functions except it accepts
   * a null terminated string instead an array.
   *
   * @param str  the base64-encoded string.
   * @return     the decoded string.
   */
  static QCString base64Decode( const QCString& str );

  /**
   * Decodes the given data that was encoded with the base64
   * algorithm.
   *
   * Use this function if you want the result of the decoding
   * to be placed in another array which cuts down the number
   * of copy operation that have to be performed in the process.
   * This is also the preferred method for decoding an encoded
   * binary data.
   *
   * NOTE: the output array is first reset and then resized
   * appropriately before use, hence, all data stored in the
   * output array will be lost.
   *
   * @param in   the encoded data to be decoded.
   * @param out  the container for the decoded data.
   */
  static void base64Decode( const QByteArray& in, QByteArray& out );


private:
  QCodecs();

private:
  static const char UUEncMap[64];
  static const char UUDecMap[128];
  static const char Base64EncMap[64];
  static const char Base64DecMap[128];
  static const char hexChars[16];
  static const unsigned int maxQPLineLength;
};

class QMD5Private;
/**
 * Provides an easy to use C++ implementation of RSA's
 * MD5 algorithm.
 *
 * The default constructor is designed to provide much the same
 * functionality as the most commonly used C-implementation, while
 * the other three constructors are meant to further simplify the
 * process of obtaining a digest by calculating the result in a
 * single step.
 *
 * QMD5 is state-based, that means you can add new contents with
 * update() as long as you didn't request the digest value yet.
 * After the digest value was requested, the object is "finalized"
 * and you have to call reset() to be able to do another calculation
 * with it.  The reason for this behaviour is that upon requesting
 * the message digest QMD5 has to pad the received contents up to a
 * 64 byte boundary to calculate its value. After this operation it
 * is not possible to resume consuming data.
 *
 * @sect Usage:
 *
 * A common usage of this class:
 *
 * <PRE>
 *  const char* test1;
 *  QMD5::Digest rawResult;
 *
 *  test1 = "This is a simple test.";
 *  QMD5 context (test1);
 *  cout << "Hex Digest output: " << context.hexDigest().data() << endl;
 * </PRE>
 *
 * To cut down on the unnecessary overhead of creating multiple QMD5
 * objects, you can simply invoke @ref reset() to reuse the same object
 * in making another calculation:
 *
 * <PRE>
 *  context.reset ();
 *  context.update ("TWO");
 *  context.update ("THREE");
 *  cout << "Hex Digest output: " << context.hexDigest().data() << endl;
 * </PRE>
 *
 * @short An adapted C++ implementation of RSA Data Securities MD5 algorithm.
 * @author Dirk Mueller <mueller@kde.org>, Dawit Alemayehu <adawit@kde.org>
 */

class QMD5
{
public:

  typedef unsigned char Digest[16];

  QMD5();

  /**
   * Constructor that updates the digest for the given string.
   *
   * @param in   C string or binary data
   * @param len  if negative, calculates the length by using
   *             strlen on the first parameter, otherwise
   *             it trusts the given length (does not stop on NUL byte).
   */
  QMD5(const char* in, int len = -1);

  /**
   * @overload
   *
   * Same as above except it accepts a QByteArray as its argument.
   */
  QMD5(const QByteArray& a );

  /**
   * @overload
   *
   * Same as above except it accepts a QByteArray as its argument.
   */
  QMD5(const QCString& a );

  /**
   * Updates the message to be digested. Be sure to add all data
   * before you read the digest. After reading the digest, you
   * can <b>not</b> add more data!
   *
   * @param in     message to be added to digest
   * @param len    the length of the given message.
   */
  void update(const char* in, int len = -1) { update(reinterpret_cast<const unsigned char*>(in), len); }

  /**
   * @overload
   */
  void update(const unsigned char* in, int len = -1);

  /**
   * @overload
   *
   * @param in     message to be added to the digest (QByteArray).
   */
  void update(const QByteArray& in );

  /**
   * @overload
   *
   * @param in     message to be added to the digest (QByteArray).
   */
  void update(const QCString& in );

  /**
   * @overload
   *
   * reads the data from an I/O device, i.e. from a file (QFile).
   *
   * NOTE that the file must be open for reading.
   *
   * @param file       a pointer to FILE as returned by calls like f{d,re}open
   *
   * @returns false if an error occured during reading.
   */
  bool update(QIODevice& file);

  /**
   * Calling this function will reset the calculated message digest.
   * Use this method to perform another message digest calculation
   * without recreating the QMD5 object.
   */
  void reset();

  /**
   * @return the raw representation of the digest
   */
  const Digest& rawDigest ();

  /**
   * Fills the given array with the binary representation of the
   * message digest.
   *
   * Use this method if you do not want to worry about making
   * copy of the digest once you obtain it.
   *
   * @param bin an array of 16 characters ( char[16] )
   */
  void rawDigest( QMD5::Digest& bin );

  /**
   * Returns the value of the calculated message digest in
   * a hexadecimal representation.
   */
  QCString hexDigest ();

  /**
   * @overload
   */
  void hexDigest(QCString&);

  /**
   * Returns the value of the calculated message digest in
   * a base64-encoded representation.
   */
  QCString base64Digest ();

  /**
   * returns true if the calculated digest for the given
   * message matches the given one.
   */
  bool verify( const QMD5::Digest& digest);

  /**
   * @overload
   */
  bool verify(const QCString&);

protected:
  /**
   *  Performs the real update work.  Note
   *  that length is implied to be 64.
   */
  void transform( const unsigned char buffer[64] );

  /**
   * finalizes the digest
   */
  void finalize();

private:
  QMD5(const QMD5& u);
  QMD5& operator=(const QMD5& md);

  void init();
  void encode( unsigned char* output, Q_UINT32 *in, Q_UINT32 len );
  void decode( Q_UINT32 *output, const unsigned char* in, Q_UINT32 len );

  Q_UINT32 rotate_left( Q_UINT32 x, Q_UINT32 n );
  Q_UINT32 F( Q_UINT32 x, Q_UINT32 y, Q_UINT32 z );
  Q_UINT32 G( Q_UINT32 x, Q_UINT32 y, Q_UINT32 z );
  Q_UINT32 H( Q_UINT32 x, Q_UINT32 y, Q_UINT32 z );
  Q_UINT32 I( Q_UINT32 x, Q_UINT32 y, Q_UINT32 z );
  void FF( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d, Q_UINT32 x,
               Q_UINT32  s, Q_UINT32 ac );
  void GG( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d, Q_UINT32 x,
                Q_UINT32 s, Q_UINT32 ac );
  void HH( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d, Q_UINT32 x,
                Q_UINT32 s, Q_UINT32 ac );
  void II( Q_UINT32& a, Q_UINT32 b, Q_UINT32 c, Q_UINT32 d, Q_UINT32 x,
             Q_UINT32 s, Q_UINT32 ac );

private:
  Q_UINT32 m_state[4];
  Q_UINT32 m_count[2];
  Q_UINT8 m_buffer[64];
  Digest m_digest;
  bool m_finalized;

  QMD5Private* d;
};
#endif
