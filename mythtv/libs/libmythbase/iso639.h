// -*- Mode: c++ -*-
#ifndef _ISO_639_2_H_
#define _ISO_639_2_H_

#include <vector>
using namespace std;

#include <QString>
#include <QStringList>
#include <QMap>

#include "mythbaseexp.h"

extern MBASE_PUBLIC QMap<int, QString> _iso639_key_to_english_name;

/** \file iso639.h
 *  \brief ISO 639-1 and ISO 639-2 support functions
 *
 *   ISO 639-1 is the two letter standard for specifying a language.
 *   This is used by MythTV for naming the themes and for initializing
 *   the Qt translation system.
 *
 *   ISO 639-2 is the three letter standard for specifying a language.
 *   This is used by MythTV for selecting subtitles and audio streams
 *   during playback, and for selecting which languages to collect
 *   EIT program guide information in.
 *
 *   In many contexts, such as with translations, these language codes can
 *   be appended with an underscore and a 2 digit IETF region code.
 *   So for Brazilian Portugese you could use: "por_BR", or "pt_BR".
 *   Or, you could specify just the language, Portugese: "por", or "pt".
 *
 **  \sa iso639.h
 */

/// Converts a 2 or 3 character iso639 string to a language name in English.
MBASE_PUBLIC  QString     iso639_str_toName(const unsigned char *iso639);
/// Converts a canonical key to language name in English
MBASE_PUBLIC  QString     iso639_key_toName(int iso639_2);
MBASE_PUBLIC  void        iso639_clear_language_list(void);
MBASE_PUBLIC  QStringList iso639_get_language_list(void);
MBASE_PUBLIC  vector<int> iso639_get_language_key_list(void);
MBASE_PUBLIC  int         iso639_key_to_canonical_key(int iso639_2);
MBASE_PUBLIC  QString     iso639_str2_to_str3(const QString &str2);

static inline QString iso639_key_to_str3(int code)
{
    char str[4];
    str[0] = (code>>16) & 0xFF;
    str[1] = (code>>8)  & 0xFF;
    str[2] = code & 0xFF;
    str[3] = 0;
    return QString(str);
}

/// Returns true if the key is 0, 0xFFFFFF, or 'und'
static inline bool iso639_is_key_undefined(int code)
{
    int bits = code & 0xFFFFFF;
    return (0 == bits) || (0xFFFFFF == bits) || (0x756E64 == bits);
}

static inline int iso639_str3_to_key(const unsigned char *iso639_2)
{
    return ((tolower(iso639_2[0])<<16)|(tolower(iso639_2[1])<<8)|tolower(iso639_2[2]));
}

static inline int iso639_str3_to_key(const char *iso639_2)
{
    return iso639_str3_to_key((const unsigned char*)iso639_2);
}

static inline int iso639_str3_to_key(const QString &iso639_2)
{
    if (iso639_2.length() < 3)
    {
        return iso639_str3_to_key("und");
    }
    else
    {
        return ((iso639_2.at(0).toLatin1()<<16) |
                (iso639_2.at(1).toLatin1()<<8) |
                (iso639_2.at(2).toLatin1()));
    }
}


static inline int iso639_str2_to_key2(const unsigned char *iso639_1)
{
    return (iso639_1[0]<<8)|iso639_1[1];
}

static inline int iso639_str2_to_key2(const char *iso639_1)
{
    return iso639_str2_to_key2((const unsigned char*)iso639_1);
}

static inline QString iso639_str_to_canonoical_str(const QString &str3)
{
    int key = iso639_str3_to_key(str3.toLatin1().constData());
    int can =  iso639_key_to_canonical_key(key);
    return iso639_key_to_str3(can);
}

MBASE_PUBLIC  QString GetISO639LanguageName(QString iso639Code);
MBASE_PUBLIC  QString GetISO639EnglishLanguageName(QString iso639Code);

#endif // _ISO_639_2_H_
