// -*- Mode: c++ -*-

#include <qmap.h>
#include <qstring.h>
#include <qstringlist.h>

extern QMap<int, QString> iso639_key_to_english_name;
extern QMap<int, int>     iso639_str2_to_str3;

/// Converts a 2 or 3 character iso639 string to a language name in English.
QString iso639_toName(const unsigned char *iso639);

QStringList iso639_get_language_list(void);

static inline QString iso639_key_to_str3(int code)
{
    char str[4];
    str[0] = (code>>16) & 0xFF;
    str[1] = (code>>8)  & 0xFF;
    str[2] = code & 0xFF;
    str[3] = 0;
    return QString(str);
}

static inline int iso639_str3_to_key(const unsigned char *iso639_2)
{
    return (iso639_2[0]<<16)|(iso639_2[1]<<8)|iso639_2[2];
}

static inline int iso639_str3_to_key(const char *iso639_2)
{
    return iso639_str3_to_key((const unsigned char*)iso639_2);
}

static inline int iso639_str2_to_key(const unsigned char *iso639_1)
{
    return (iso639_1[1]<<8)|iso639_1[1];
}

static inline int iso639_str2_to_key(const char *iso639_1)
{
    return iso639_str2_to_key((const unsigned char*)iso639_1);
}
