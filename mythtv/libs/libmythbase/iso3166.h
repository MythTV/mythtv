
// -*- Mode: c++ -*-
#ifndef _ISO_3166_1_H_
#define _ISO_3166_1_H_

#include <vector>
using namespace std;

#include <QString>
#include <QStringList>
#include <QMap>

#include "mythbaseexp.h"

extern MBASE_PUBLIC QMap<int, QString> _iso3166_key_to_english_name;

/** \file iso3166.h
 *  \brief ISO 3166-1 support functions
 *
 *   ISO 3166-1 alpha-2 is the two letter standard for specifying a country.
 *   This is used by MythTV for locale support.
 *
 *   In many contexts, such as with translations, these country codes can
 *   be prefixed with a 2 digit ISO639 language code and an underscore.
 *
 *   \sa iso639.h
 */

typedef QMap<QString, QString> CodeToNameMap;

 MBASE_PUBLIC  CodeToNameMap GetISO3166EnglishCountryMap(void);
 MBASE_PUBLIC  QString GetISO3166EnglishCountryName(QString iso3166Code);
 MBASE_PUBLIC  CodeToNameMap GetISO3166CountryMap();
 MBASE_PUBLIC  QString GetISO3166CountryName(QString iso3166Code);

#endif // _ISO_3166_1_H_
