#ifndef MD5_H
#define MD5_H

/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Changed so as no longer to depend on Colin Plumb's `usual.h'
 * header definitions; now uses stuff from dpkg's config.h
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 */

#define HASHLEN 16
typedef char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN + 1];
#ifdef WIN32
#undef IN
#endif
#define IN const
#define OUT


/* calculate H(A1) as per HTTP Digest spec */
extern void
    DigestCalcHA1(
        IN char * pszAlg,
        IN char * pszUserName,
        IN char * pszRealm,
        IN char * pszPassword,
        IN char * pszNonce,
        IN char * pszCNonce,
        OUT HASHHEX SessionKey
    );

/* calculate request-digest/response-digest as per HTTP Digest spec */
extern void
    DigestCalcResponse(
        IN HASHHEX HA1,            /* H(A1) */
        IN char * pszNonce,        /* nonce from server */
        IN char * pszNonceCount,   /* 8 hex digits */
        IN char * pszCNonce,       /* client nonce */
        IN char * pszQop,          /* qop-value: "", "auth", "auth-int" */
        IN char * pszMethod,       /* method from the request */
        IN char * pszDigestUri,    /* requested URL */
        IN HASHHEX HEntity,        /* H(entity body) if qop="auth-int" */
        OUT HASHHEX HA2Hex,        /* H(A2) */
        OUT HASHHEX Response      /* request-digest or response-digest */
    );



extern void
    CvtHex(
        IN HASH Bin,
        OUT HASHHEX Hex
    );


#ifdef __cplusplus
extern "C"
{
#endif

#define md5byte unsigned char

    struct MD5Context
    {
        unsigned int buf[4];
        unsigned int bytes[2];
        unsigned int in[16];
    };

    void MD5Init(struct MD5Context *context);
    void MD5Update(struct MD5Context *context, md5byte const *buf, unsigned len);
    void MD5Final(unsigned char digest[16], struct MD5Context *context);
    void MD5Transform(unsigned int buf[4], unsigned int const in[16]);

#ifdef __cplusplus
}
#endif

/* MD5_H */
#endif
