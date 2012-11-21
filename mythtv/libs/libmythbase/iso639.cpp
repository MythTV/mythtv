// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include "iso639.h"
#include "mythcorecontext.h"

QMap<int, QString>    _iso639_key_to_english_name;
static QMap<int, int> _iso639_key2_to_key3;
static QMap<int, int> _iso639_key3_to_canonical_key3;
static QStringList    _languages;
static vector<int>    _language_keys;

/* Note: this file takes a long time to compile. **/

static int createCodeToEnglishNamesMap(QMap<int, QString>& names);
static int createCode2ToCode3Map(QMap<int, int>& codemap);
static int createCodeToCanonicalCodeMap(QMap<int, int>& canonical);

void iso639_clear_language_list(void)
{
    _languages.clear();
    _language_keys.clear();
}

/** \fn QStringList iso639_get_language_list(void)
 *  \brief Returns list of three character ISO-639 language
 *         descriptors, starting with the most preferred.
 *  \sa MythContext::GetLanguage()
 */
QStringList iso639_get_language_list(void)
{
    if (_languages.empty())
    {
        for (uint i = 0; true; i++)
        {
            QString q = QString("ISO639Language%1").arg(i);
            QString lang = gCoreContext->GetSetting(q, "").toLower();
            if (lang.isEmpty())
                break;
            _languages << lang;
        }
        if (_languages.empty())
        {
            QString s3 = iso639_str2_to_str3(
                                        gCoreContext->GetLanguage().toLower());
            if (!s3.isEmpty())
                _languages << s3;
        }
    }
    return _languages;
}

vector<int> iso639_get_language_key_list(void)
{
    if (_language_keys.empty())
    {
        const QStringList list = iso639_get_language_list();
        QStringList::const_iterator it = list.begin();
        for (; it != list.end(); ++it)
            _language_keys.push_back(iso639_str3_to_key(*it));
    }
    return _language_keys;
}

QString iso639_str2_to_str3(const QString &str2)
{
    int key2 = iso639_str2_to_key2(str2.toAscii().constData());
    int key3 = 0;
    if (_iso639_key2_to_key3.contains(key2))
        key3 = _iso639_key2_to_key3[key2];
    if (key3)
        return iso639_key_to_str3(key3);
    return "und";
}

static QString iso639_Alpha3_toName(const unsigned char *iso639_2)
{
    int alpha3 = iso639_str3_to_key(iso639_2);
    alpha3 = iso639_key_to_canonical_key(alpha3);

    if (_iso639_key_to_english_name.contains(alpha3))
        return _iso639_key_to_english_name[alpha3];

    return "Unknown";
}

static QString iso639_Alpha2_toName(const unsigned char *iso639_1)
{
    int alpha2 = iso639_str2_to_key2(iso639_1);

    if (_iso639_key2_to_key3.contains(alpha2))
        return _iso639_key_to_english_name[_iso639_key2_to_key3[alpha2]];

    return "Unknown";
}

QString iso639_str_toName(const unsigned char *iso639)
{
    if (strlen((const char *)iso639) == 2)
        return iso639_Alpha2_toName(iso639);
    else if (strlen((const char *)iso639) == 3)
        return iso639_Alpha3_toName(iso639);

    return "Unknown";
}

QString iso639_key_toName(int iso639_2)
{
    QMap<int, QString>::const_iterator it;
    it = _iso639_key_to_english_name.find(iso639_2);
    if (it != _iso639_key_to_english_name.end())
        return *it;

    return "Unknown";
}

int iso639_key_to_canonical_key(int iso639_2)
{
    QMap<int, int>::const_iterator it;
    it = _iso639_key3_to_canonical_key3.find(iso639_2);

    if (it != _iso639_key3_to_canonical_key3.end())
        return *it;
    return iso639_2;
}

int dummy_createCodeToEnglishNamesMap =
    createCodeToEnglishNamesMap(_iso639_key_to_english_name);

int dummy_createCode2ToCode3Map =
    createCode2ToCode3Map(_iso639_key2_to_key3);

int dummy_createCodeToCanonicalCodeMap =
    createCodeToCanonicalCodeMap(_iso639_key3_to_canonical_key3);

static int createCodeToCanonicalCodeMap(QMap<int, int>& canonical)
{
    canonical[iso639_str3_to_key("sqi")] = iso639_str3_to_key("alb");
    canonical[iso639_str3_to_key("hye")] = iso639_str3_to_key("arm");
    canonical[iso639_str3_to_key("eus")] = iso639_str3_to_key("baq");
    canonical[iso639_str3_to_key("mya")] = iso639_str3_to_key("bur");
    canonical[iso639_str3_to_key("zho")] = iso639_str3_to_key("chi");
    canonical[iso639_str3_to_key("ces")] = iso639_str3_to_key("cze");
    canonical[iso639_str3_to_key("nld")] = iso639_str3_to_key("dut");
    canonical[iso639_str3_to_key("fra")] = iso639_str3_to_key("fre");
    canonical[iso639_str3_to_key("kat")] = iso639_str3_to_key("geo");
    canonical[iso639_str3_to_key("deu")] = iso639_str3_to_key("ger");
    canonical[iso639_str3_to_key("ell")] = iso639_str3_to_key("gre");
    canonical[iso639_str3_to_key("isl")] = iso639_str3_to_key("ice");
    canonical[iso639_str3_to_key("mkd")] = iso639_str3_to_key("mac");
    canonical[iso639_str3_to_key("mri")] = iso639_str3_to_key("mao");
    canonical[iso639_str3_to_key("msa")] = iso639_str3_to_key("may");
    canonical[iso639_str3_to_key("fas")] = iso639_str3_to_key("per");
    canonical[iso639_str3_to_key("ron")] = iso639_str3_to_key("rum");
    canonical[iso639_str3_to_key("srp")] = iso639_str3_to_key("scc");
    canonical[iso639_str3_to_key("hrv")] = iso639_str3_to_key("scr");
    canonical[iso639_str3_to_key("slk")] = iso639_str3_to_key("slo");
    canonical[iso639_str3_to_key("bod")] = iso639_str3_to_key("tib");
    canonical[iso639_str3_to_key("cym")] = iso639_str3_to_key("wel");
    return 0;
}

/** \fn createCodeToEnglishNamesMap(QMap<int, QString>&)
  Generated from
    http://www.loc.gov/standards/iso639-2/ascii_8bits.html
  using awk script:
    cat ISO-639-2_values_8bits.txt | \
      awk -F'|' \
      '{printf "  names[iso639_str3_to_key(\"%s\")] = QString(\"%s\");\n", \
        $1, $4}'

  with hand editing for duplicates ("ger"=="deu").
*/
static int createCodeToEnglishNamesMap(QMap<int, QString>& names)
{
  names[iso639_str3_to_key("aar")] = QString("Afar");
  names[iso639_str3_to_key("abk")] = QString("Abkhazian");
  names[iso639_str3_to_key("ace")] = QString("Achinese");
  names[iso639_str3_to_key("ach")] = QString("Acoli");
  names[iso639_str3_to_key("ada")] = QString("Adangme");
  names[iso639_str3_to_key("ady")] = QString("Adyghe; Adygei");
  names[iso639_str3_to_key("afa")] = QString("Afro-Asiatic (Other)");
  names[iso639_str3_to_key("afh")] = QString("Afrihili");
  names[iso639_str3_to_key("afr")] = QString("Afrikaans");
  names[iso639_str3_to_key("aka")] = QString("Akan");
  names[iso639_str3_to_key("akk")] = QString("Akkadian");
  names[iso639_str3_to_key("alb")] = QString("Albanian"); // sqi
  names[iso639_str3_to_key("ale")] = QString("Aleut");
  names[iso639_str3_to_key("alg")] = QString("Algonquian languages");
  names[iso639_str3_to_key("amh")] = QString("Amharic");
  names[iso639_str3_to_key("ang")] = QString("Old English (ca.450-1100)");
  names[iso639_str3_to_key("apa")] = QString("Apache languages");
  names[iso639_str3_to_key("ara")] = QString("Arabic");
  names[iso639_str3_to_key("arc")] = QString("Aramaic");
  names[iso639_str3_to_key("arg")] = QString("Aragonese");
  names[iso639_str3_to_key("arm")] = QString("Armenian"); // hye
  names[iso639_str3_to_key("arn")] = QString("Araucanian");
  names[iso639_str3_to_key("arp")] = QString("Arapaho");
  names[iso639_str3_to_key("art")] = QString("Artificial (Other)");
  names[iso639_str3_to_key("arw")] = QString("Arawak");
  names[iso639_str3_to_key("asm")] = QString("Assamese");
  names[iso639_str3_to_key("ast")] = QString("Asturian; Bable");
  names[iso639_str3_to_key("ath")] = QString("Athapascan languages");
  names[iso639_str3_to_key("aus")] = QString("Australian languages");
  names[iso639_str3_to_key("ava")] = QString("Avaric");
  names[iso639_str3_to_key("ave")] = QString("Avestan");
  names[iso639_str3_to_key("awa")] = QString("Awadhi");
  names[iso639_str3_to_key("aym")] = QString("Aymara");
  names[iso639_str3_to_key("aze")] = QString("Azerbaijani");
  names[iso639_str3_to_key("bad")] = QString("Banda");
  names[iso639_str3_to_key("bai")] = QString("Bamileke languages");
  names[iso639_str3_to_key("bak")] = QString("Bashkir");
  names[iso639_str3_to_key("bal")] = QString("Baluchi");
  names[iso639_str3_to_key("bam")] = QString("Bambara");
  names[iso639_str3_to_key("ban")] = QString("Balinese");
  names[iso639_str3_to_key("baq")] = QString("Basque"); // eus
  names[iso639_str3_to_key("bas")] = QString("Basa");
  names[iso639_str3_to_key("bat")] = QString("Baltic (Other)");
  names[iso639_str3_to_key("bej")] = QString("Beja");
  names[iso639_str3_to_key("bel")] = QString("Belarusian");
  names[iso639_str3_to_key("bem")] = QString("Bemba");
  names[iso639_str3_to_key("ben")] = QString("Bengali");
  names[iso639_str3_to_key("ber")] = QString("Berber (Other)");
  names[iso639_str3_to_key("bho")] = QString("Bhojpuri");
  names[iso639_str3_to_key("bih")] = QString("Bihari");
  names[iso639_str3_to_key("bik")] = QString("Bikol");
  names[iso639_str3_to_key("bin")] = QString("Bini");
  names[iso639_str3_to_key("bis")] = QString("Bislama");
  names[iso639_str3_to_key("bla")] = QString("Siksika");
  names[iso639_str3_to_key("bnt")] = QString("Bantu (Other)");
  names[iso639_str3_to_key("bos")] = QString("Bosnian");
  names[iso639_str3_to_key("bra")] = QString("Braj");
  names[iso639_str3_to_key("bre")] = QString("Breton");
  names[iso639_str3_to_key("btk")] = QString("Batak (Indonesia)");
  names[iso639_str3_to_key("bua")] = QString("Buriat");
  names[iso639_str3_to_key("bug")] = QString("Buginese");
  names[iso639_str3_to_key("bul")] = QString("Bulgarian");
  names[iso639_str3_to_key("bur")] = QString("Burmese");
  names[iso639_str3_to_key("bur")] = QString("Burmese");
  names[iso639_str3_to_key("byn")] = QString("Blin; Bilin");
  names[iso639_str3_to_key("cad")] = QString("Caddo");
  names[iso639_str3_to_key("cai")] = QString("Central American Indian (Other)");
  names[iso639_str3_to_key("car")] = QString("Carib");
  names[iso639_str3_to_key("cat")] = QString("Catalan; Valencian");
  names[iso639_str3_to_key("cau")] = QString("Caucasian (Other)");
  names[iso639_str3_to_key("ceb")] = QString("Cebuano");
  names[iso639_str3_to_key("cel")] = QString("Celtic (Other)");
  names[iso639_str3_to_key("cha")] = QString("Chamorro");
  names[iso639_str3_to_key("chb")] = QString("Chibcha");
  names[iso639_str3_to_key("che")] = QString("Chechen");
  names[iso639_str3_to_key("chg")] = QString("Chagatai");
  names[iso639_str3_to_key("chi")] = QString("Chinese"); // zho
  names[iso639_str3_to_key("chk")] = QString("Chuukese");
  names[iso639_str3_to_key("chm")] = QString("Mari");
  names[iso639_str3_to_key("chn")] = QString("Chinook jargon");
  names[iso639_str3_to_key("cho")] = QString("Choctaw");
  names[iso639_str3_to_key("chp")] = QString("Chipewyan");
  names[iso639_str3_to_key("chr")] = QString("Cherokee");
  names[iso639_str3_to_key("chu")] = QString("Church Slavic; Old Slavonic").append(
      QString("; Church Slavonic; Old Bulgarian; Old Church Slavonic"));
  names[iso639_str3_to_key("chv")] = QString("Chuvash");
  names[iso639_str3_to_key("chy")] = QString("Cheyenne");
  names[iso639_str3_to_key("cmc")] = QString("Chamic languages");
  names[iso639_str3_to_key("cop")] = QString("Coptic");
  names[iso639_str3_to_key("cor")] = QString("Cornish");
  names[iso639_str3_to_key("cos")] = QString("Corsican");
  names[iso639_str3_to_key("cpe")] = QString("Creoles and pidgins, English based (Other)");
  names[iso639_str3_to_key("cpf")] = QString("Creoles and pidgins, French-based (Other)");
  names[iso639_str3_to_key("cpp")] = QString("Creoles and pidgins, Portuguese-based (Other)");
  names[iso639_str3_to_key("cre")] = QString("Cree");
  names[iso639_str3_to_key("crh")] = QString("Crimean Tatar; Crimean Turkish");
  names[iso639_str3_to_key("crp")] = QString("Creoles and pidgins (Other)");
  names[iso639_str3_to_key("csb")] = QString("Kashubian");
  names[iso639_str3_to_key("cus")] = QString("Cushitic (Other)");
  names[iso639_str3_to_key("cze")] = QString("Czech"); // ces
  names[iso639_str3_to_key("dak")] = QString("Dakota");
  names[iso639_str3_to_key("dan")] = QString("Danish");
  names[iso639_str3_to_key("dar")] = QString("Dargwa");
  names[iso639_str3_to_key("day")] = QString("Dayak");
  names[iso639_str3_to_key("del")] = QString("Delaware");
  names[iso639_str3_to_key("den")] = QString("Slave (Athapascan)");
  names[iso639_str3_to_key("dgr")] = QString("Dogrib");
  names[iso639_str3_to_key("din")] = QString("Dinka");
  names[iso639_str3_to_key("div")] = QString("Divehi");
  names[iso639_str3_to_key("doi")] = QString("Dogri");
  names[iso639_str3_to_key("dra")] = QString("Dravidian (Other)");
  names[iso639_str3_to_key("dsb")] = QString("Lower Sorbian");
  names[iso639_str3_to_key("dua")] = QString("Duala");
  names[iso639_str3_to_key("dum")] = QString("Middle Dutch (ca.1050-1350)");
  names[iso639_str3_to_key("dut")] = QString("Dutch; Flemish"); // nld
  names[iso639_str3_to_key("dyu")] = QString("Dyula");
  names[iso639_str3_to_key("dzo")] = QString("Dzongkha");
  names[iso639_str3_to_key("efi")] = QString("Efik");
  names[iso639_str3_to_key("egy")] = QString("Egyptian (Ancient)");
  names[iso639_str3_to_key("eka")] = QString("Ekajuk");
  names[iso639_str3_to_key("elx")] = QString("Elamite");
  names[iso639_str3_to_key("eng")] = QString("English");
  names[iso639_str3_to_key("enm")] = QString("Middle English (1100-1500)");
  names[iso639_str3_to_key("epo")] = QString("Esperanto");
  names[iso639_str3_to_key("est")] = QString("Estonian");
  names[iso639_str3_to_key("ewe")] = QString("Ewe");
  names[iso639_str3_to_key("ewo")] = QString("Ewondo");
  names[iso639_str3_to_key("fan")] = QString("Fang");
  names[iso639_str3_to_key("fao")] = QString("Faroese");
  names[iso639_str3_to_key("fat")] = QString("Fanti");
  names[iso639_str3_to_key("fij")] = QString("Fijian");
  names[iso639_str3_to_key("fin")] = QString("Finnish");
  names[iso639_str3_to_key("fiu")] = QString("Finno-Ugrian (Other)");
  names[iso639_str3_to_key("fon")] = QString("Fon");
  names[iso639_str3_to_key("fre")] = QString("French");
  names[iso639_str3_to_key("frm")] = QString("Middle French (ca.1400-1800)");
  names[iso639_str3_to_key("fro")] = QString("Old French (842-ca.1400)");
  names[iso639_str3_to_key("fry")] = QString("Frisian");
  names[iso639_str3_to_key("ful")] = QString("Fulah");
  names[iso639_str3_to_key("fur")] = QString("Friulian");
  names[iso639_str3_to_key("gaa")] = QString("Ga");
  names[iso639_str3_to_key("gay")] = QString("Gayo");
  names[iso639_str3_to_key("gba")] = QString("Gbaya");
  names[iso639_str3_to_key("gem")] = QString("Germanic (Other)");
  names[iso639_str3_to_key("geo")] = QString("Georgian"); // kat
  names[iso639_str3_to_key("ger")] = QString("German"); // deu
  names[iso639_str3_to_key("gez")] = QString("Geez");
  names[iso639_str3_to_key("gil")] = QString("Gilbertese");
  names[iso639_str3_to_key("gla")] = QString("Gaelic; Scottish Gaelic");
  names[iso639_str3_to_key("gle")] = QString("Irish");
  names[iso639_str3_to_key("glg")] = QString("Gallegan");
  names[iso639_str3_to_key("glv")] = QString("Manx");
  names[iso639_str3_to_key("gmh")] = QString("Middle High German (ca.1050-1500)");
  names[iso639_str3_to_key("goh")] = QString("Old High German (ca.750-1050)");
  names[iso639_str3_to_key("gon")] = QString("Gondi");
  names[iso639_str3_to_key("gor")] = QString("Gorontalo");
  names[iso639_str3_to_key("got")] = QString("Gothic");
  names[iso639_str3_to_key("grb")] = QString("Grebo");
  names[iso639_str3_to_key("grc")] = QString("Greek, Ancient (to 1453)");
  names[iso639_str3_to_key("gre")] = QString("Greek, Modern (1453-)"); // ell
  names[iso639_str3_to_key("grn")] = QString("Guarani");
  names[iso639_str3_to_key("guj")] = QString("Gujarati");
  names[iso639_str3_to_key("gwi")] = QString("Gwich�in");
  names[iso639_str3_to_key("hai")] = QString("Haida");
  names[iso639_str3_to_key("hat")] = QString("Haitian; Haitian Creole");
  names[iso639_str3_to_key("hau")] = QString("Hausa");
  names[iso639_str3_to_key("haw")] = QString("Hawaiian");
  names[iso639_str3_to_key("heb")] = QString("Hebrew");
  names[iso639_str3_to_key("her")] = QString("Herero");
  names[iso639_str3_to_key("hil")] = QString("Hiligaynon");
  names[iso639_str3_to_key("him")] = QString("Himachali");
  names[iso639_str3_to_key("hin")] = QString("Hindi");
  names[iso639_str3_to_key("hit")] = QString("Hittite");
  names[iso639_str3_to_key("hmn")] = QString("Hmong");
  names[iso639_str3_to_key("hmo")] = QString("Hiri Motu");
  names[iso639_str3_to_key("hsb")] = QString("Upper Sorbian");
  names[iso639_str3_to_key("hun")] = QString("Hungarian");
  names[iso639_str3_to_key("hup")] = QString("Hupa");
  names[iso639_str3_to_key("iba")] = QString("Iban");
  names[iso639_str3_to_key("ibo")] = QString("Igbo");
  names[iso639_str3_to_key("ice")] = QString("Icelandic"); // isl
  names[iso639_str3_to_key("ido")] = QString("Ido");
  names[iso639_str3_to_key("iii")] = QString("Sichuan Yi");
  names[iso639_str3_to_key("ijo")] = QString("Ijo");
  names[iso639_str3_to_key("iku")] = QString("Inuktitut");
  names[iso639_str3_to_key("ile")] = QString("Interlingue");
  names[iso639_str3_to_key("ilo")] = QString("Iloko");
  names[iso639_str3_to_key("ina")] = QString("Interlingua");
  names[iso639_str3_to_key("inc")] = QString("Indic (Other)");
  names[iso639_str3_to_key("ind")] = QString("Indonesian");
  names[iso639_str3_to_key("ine")] = QString("Indo-European (Other)");
  names[iso639_str3_to_key("inh")] = QString("Ingush");
  names[iso639_str3_to_key("ipk")] = QString("Inupiaq");
  names[iso639_str3_to_key("ira")] = QString("Iranian (Other)");
  names[iso639_str3_to_key("iro")] = QString("Iroquoian languages");
  names[iso639_str3_to_key("ita")] = QString("Italian");
  names[iso639_str3_to_key("jav")] = QString("Javanese");
  names[iso639_str3_to_key("jbo")] = QString("Lojban");
  names[iso639_str3_to_key("jpn")] = QString("Japanese");
  names[iso639_str3_to_key("jpr")] = QString("Judeo-Persian");
  names[iso639_str3_to_key("jrb")] = QString("Judeo-Arabic");
  names[iso639_str3_to_key("kaa")] = QString("Kara-Kalpak");
  names[iso639_str3_to_key("kab")] = QString("Kabyle");
  names[iso639_str3_to_key("kac")] = QString("Kachin");
  names[iso639_str3_to_key("kal")] = QString("Kalaallisut; Greenlandic");
  names[iso639_str3_to_key("kam")] = QString("Kamba");
  names[iso639_str3_to_key("kan")] = QString("Kannada");
  names[iso639_str3_to_key("kar")] = QString("Karen");
  names[iso639_str3_to_key("kas")] = QString("Kashmiri");
  names[iso639_str3_to_key("kau")] = QString("Kanuri");
  names[iso639_str3_to_key("kaw")] = QString("Kawi");
  names[iso639_str3_to_key("kaz")] = QString("Kazakh");
  names[iso639_str3_to_key("kbd")] = QString("Kabardian");
  names[iso639_str3_to_key("kha")] = QString("Khasi");
  names[iso639_str3_to_key("khi")] = QString("Khoisan (Other)");
  names[iso639_str3_to_key("khm")] = QString("Khmer");
  names[iso639_str3_to_key("kho")] = QString("Khotanese");
  names[iso639_str3_to_key("kik")] = QString("Kikuyu; Gikuyu");
  names[iso639_str3_to_key("kin")] = QString("Kinyarwanda");
  names[iso639_str3_to_key("kir")] = QString("Kirghiz");
  names[iso639_str3_to_key("kmb")] = QString("Kimbundu");
  names[iso639_str3_to_key("kok")] = QString("Konkani");
  names[iso639_str3_to_key("kom")] = QString("Komi");
  names[iso639_str3_to_key("kon")] = QString("Kongo");
  names[iso639_str3_to_key("kor")] = QString("Korean");
  names[iso639_str3_to_key("kos")] = QString("Kosraean");
  names[iso639_str3_to_key("kpe")] = QString("Kpelle");
  names[iso639_str3_to_key("krc")] = QString("Karachay-Balkar");
  names[iso639_str3_to_key("kro")] = QString("Kru");
  names[iso639_str3_to_key("kru")] = QString("Kurukh");
  names[iso639_str3_to_key("kua")] = QString("Kuanyama; Kwanyama");
  names[iso639_str3_to_key("kum")] = QString("Kumyk");
  names[iso639_str3_to_key("kur")] = QString("Kurdish");
  names[iso639_str3_to_key("kut")] = QString("Kutenai");
  names[iso639_str3_to_key("lad")] = QString("Ladino");
  names[iso639_str3_to_key("lah")] = QString("Lahnda");
  names[iso639_str3_to_key("lam")] = QString("Lamba");
  names[iso639_str3_to_key("lao")] = QString("Lao");
  names[iso639_str3_to_key("lat")] = QString("Latin");
  names[iso639_str3_to_key("lav")] = QString("Latvian");
  names[iso639_str3_to_key("lez")] = QString("Lezghian");
  names[iso639_str3_to_key("lim")] = QString("Limburgan; Limburger; Limburgish");
  names[iso639_str3_to_key("lin")] = QString("Lingala");
  names[iso639_str3_to_key("lit")] = QString("Lithuanian");
  names[iso639_str3_to_key("lol")] = QString("Mongo");
  names[iso639_str3_to_key("loz")] = QString("Lozi");
  names[iso639_str3_to_key("ltz")] = QString("Luxembourgish; Letzeburgesch");
  names[iso639_str3_to_key("lua")] = QString("Luba-Lulua");
  names[iso639_str3_to_key("lub")] = QString("Luba-Katanga");
  names[iso639_str3_to_key("lug")] = QString("Ganda");
  names[iso639_str3_to_key("lui")] = QString("Luiseno");
  names[iso639_str3_to_key("lun")] = QString("Lunda");
  names[iso639_str3_to_key("luo")] = QString("Luo (Kenya and Tanzania)");
  names[iso639_str3_to_key("lus")] = QString("lushai");
  names[iso639_str3_to_key("mac")] = QString("Macedonian"); // mkd
  names[iso639_str3_to_key("mad")] = QString("Madurese");
  names[iso639_str3_to_key("mag")] = QString("Magahi");
  names[iso639_str3_to_key("mah")] = QString("Marshallese");
  names[iso639_str3_to_key("mai")] = QString("Maithili");
  names[iso639_str3_to_key("mak")] = QString("Makasar");
  names[iso639_str3_to_key("mal")] = QString("Malayalam");
  names[iso639_str3_to_key("man")] = QString("Mandingo");
  names[iso639_str3_to_key("mao")] = QString("Maori"); // mri
  names[iso639_str3_to_key("map")] = QString("Austronesian (Other)");
  names[iso639_str3_to_key("mar")] = QString("Marathi");
  names[iso639_str3_to_key("mas")] = QString("Masai");
  names[iso639_str3_to_key("may")] = QString("Malay"); // msa
  names[iso639_str3_to_key("mdf")] = QString("Moksha");
  names[iso639_str3_to_key("mdr")] = QString("Mandar");
  names[iso639_str3_to_key("men")] = QString("Mende");
  names[iso639_str3_to_key("mga")] = QString("Middle Irish (900-1200)");
  names[iso639_str3_to_key("mic")] = QString("Micmac");
  names[iso639_str3_to_key("min")] = QString("Minangkabau");
  names[iso639_str3_to_key("mis")] = QString("Miscellaneous languages");
  names[iso639_str3_to_key("mkh")] = QString("Mon-Khmer (Other)");
  names[iso639_str3_to_key("mlg")] = QString("Malagasy");
  names[iso639_str3_to_key("mlt")] = QString("Maltese");
  names[iso639_str3_to_key("mnc")] = QString("Manchu");
  names[iso639_str3_to_key("mni")] = QString("Manipuri");
  names[iso639_str3_to_key("mno")] = QString("Manobo languages");
  names[iso639_str3_to_key("moh")] = QString("Mohawk");
  names[iso639_str3_to_key("mol")] = QString("Moldavian");
  names[iso639_str3_to_key("mon")] = QString("Mongolian");
  names[iso639_str3_to_key("mos")] = QString("Mossi");
  names[iso639_str3_to_key("mul")] = QString("Multiple languages");
  names[iso639_str3_to_key("mun")] = QString("Munda languages");
  names[iso639_str3_to_key("mus")] = QString("Creek");
  names[iso639_str3_to_key("mwr")] = QString("Marwari");
  names[iso639_str3_to_key("myn")] = QString("Mayan languages");
  names[iso639_str3_to_key("myv")] = QString("Erzya");
  names[iso639_str3_to_key("nar")] = QString("Narrative"); // UK & Irish DTV Spec
  names[iso639_str3_to_key("nah")] = QString("Nahuatl");
  names[iso639_str3_to_key("nai")] = QString("North American Indian");
  names[iso639_str3_to_key("nap")] = QString("Neapolitan");
  names[iso639_str3_to_key("nau")] = QString("Nauru");
  names[iso639_str3_to_key("nav")] = QString("Navajo; Navaho");
  names[iso639_str3_to_key("nbl")] = QString("Ndebele, South; South Ndebele");
  names[iso639_str3_to_key("nde")] = QString("Ndebele, North; North Ndebele");
  names[iso639_str3_to_key("ndo")] = QString("Ndonga");
  names[iso639_str3_to_key("nds")] = QString("Low German; Low Saxon");
  names[iso639_str3_to_key("nep")] = QString("Nepali");
  names[iso639_str3_to_key("new")] = QString("Newari");
  names[iso639_str3_to_key("nia")] = QString("Nias");
  names[iso639_str3_to_key("nic")] = QString("Niger-Kordofanian (Other)");
  names[iso639_str3_to_key("niu")] = QString("Niuean");
  names[iso639_str3_to_key("nno")] = QString("Norwegian Nynorsk");
  names[iso639_str3_to_key("nob")] = QString("Norwegian Bokm�l");
  names[iso639_str3_to_key("nog")] = QString("Nogai");
  names[iso639_str3_to_key("non")] = QString("Old Norse");
  names[iso639_str3_to_key("nor")] = QString("Norwegian");
  names[iso639_str3_to_key("nso")] = QString("Northern Sotho");
  names[iso639_str3_to_key("nub")] = QString("Nubian languages");
  names[iso639_str3_to_key("nwc")] = QString("Classical Newari; Old Newari");
  names[iso639_str3_to_key("nya")] = QString("Chichewa; Chewa; Nyanja");
  names[iso639_str3_to_key("nym")] = QString("Nyamwezi");
  names[iso639_str3_to_key("nyn")] = QString("Nyankole");
  names[iso639_str3_to_key("nyo")] = QString("Nyoro");
  names[iso639_str3_to_key("nzi")] = QString("Nzima");
  names[iso639_str3_to_key("oci")] = QString("Occitan (post 1500); Proven�al");
  names[iso639_str3_to_key("oji")] = QString("Ojibwa");
  names[iso639_str3_to_key("ori")] = QString("Oriya");
  names[iso639_str3_to_key("orm")] = QString("Oromo");
  names[iso639_str3_to_key("osa")] = QString("Osage");
  names[iso639_str3_to_key("oss")] = QString("Ossetian; Ossetic");
  names[iso639_str3_to_key("ota")] = QString("Ottoman Turkish (1500-1928)");
  names[iso639_str3_to_key("oto")] = QString("Otomian languages");
  names[iso639_str3_to_key("paa")] = QString("Papuan (Other)");
  names[iso639_str3_to_key("pag")] = QString("Pangasinan");
  names[iso639_str3_to_key("pal")] = QString("Pahlavi");
  names[iso639_str3_to_key("pam")] = QString("Pampanga");
  names[iso639_str3_to_key("pan")] = QString("Panjabi; Punjabi");
  names[iso639_str3_to_key("pap")] = QString("Papiamento");
  names[iso639_str3_to_key("pau")] = QString("Palauan");
  names[iso639_str3_to_key("peo")] = QString("Old Persian (ca.600-400 B.C.)");
  names[iso639_str3_to_key("per")] = QString("Persian"); // fas
  names[iso639_str3_to_key("phi")] = QString("Philippine (Other)");
  names[iso639_str3_to_key("phn")] = QString("Phoenician");
  names[iso639_str3_to_key("pli")] = QString("Pali");
  names[iso639_str3_to_key("pol")] = QString("Polish");
  names[iso639_str3_to_key("pon")] = QString("Pohnpeian");
  names[iso639_str3_to_key("por")] = QString("Portuguese");
  names[iso639_str3_to_key("pra")] = QString("Prakrit languages");
  names[iso639_str3_to_key("pro")] = QString("Old Proven�al  (to 1500)");
  names[iso639_str3_to_key("pus")] = QString("Pushto");
  names[iso639_str3_to_key("qaa")] = QString("Original language"); // from DVB-SI (EN 300 468)
  names[iso639_str3_to_key("qtz")] = QString("Reserved for local use");
  names[iso639_str3_to_key("que")] = QString("Quechua");
  names[iso639_str3_to_key("raj")] = QString("Rajasthani");
  names[iso639_str3_to_key("rap")] = QString("Rapanui");
  names[iso639_str3_to_key("rar")] = QString("Rarotongan");
  names[iso639_str3_to_key("roa")] = QString("Romance (Other)");
  names[iso639_str3_to_key("roh")] = QString("Raeto-Romance");
  names[iso639_str3_to_key("rom")] = QString("Romany");
  names[iso639_str3_to_key("rum")] = QString("Romanian");
  names[iso639_str3_to_key("run")] = QString("Rundi");
  names[iso639_str3_to_key("rus")] = QString("Russian");
  names[iso639_str3_to_key("sad")] = QString("Sandawe");
  names[iso639_str3_to_key("sag")] = QString("Sango");
  names[iso639_str3_to_key("sah")] = QString("Yakut");
  names[iso639_str3_to_key("sai")] = QString("South American Indian (Other)");
  names[iso639_str3_to_key("sal")] = QString("Salishan languages");
  names[iso639_str3_to_key("sam")] = QString("Samaritan Aramaic");
  names[iso639_str3_to_key("san")] = QString("Sanskrit");
  names[iso639_str3_to_key("sas")] = QString("Sasak");
  names[iso639_str3_to_key("sat")] = QString("Santali");
  names[iso639_str3_to_key("scc")] = QString("Serbian"); // srp
  names[iso639_str3_to_key("sco")] = QString("Scots");
  names[iso639_str3_to_key("scr")] = QString("Croatian"); // hrv
  names[iso639_str3_to_key("sel")] = QString("Selkup");
  names[iso639_str3_to_key("sem")] = QString("Semitic (Other)");
  names[iso639_str3_to_key("sga")] = QString("Old Irish (to 900)");
  names[iso639_str3_to_key("sgn")] = QString("Sign Languages");
  names[iso639_str3_to_key("shn")] = QString("Shan");
  names[iso639_str3_to_key("sid")] = QString("Sidamo");
  names[iso639_str3_to_key("sin")] = QString("Sinhalese");
  names[iso639_str3_to_key("sio")] = QString("Siouan languages");
  names[iso639_str3_to_key("sit")] = QString("Sino-Tibetan (Other)");
  names[iso639_str3_to_key("sla")] = QString("Slavic (Other)");
  names[iso639_str3_to_key("slo")] = QString("Slovak"); // slk
  names[iso639_str3_to_key("slv")] = QString("Slovenian");
  names[iso639_str3_to_key("sma")] = QString("Southern Sami");
  names[iso639_str3_to_key("sme")] = QString("Northern Sami");
  names[iso639_str3_to_key("smi")] = QString("Sami languages (Other)");
  names[iso639_str3_to_key("smj")] = QString("Lule Sami");
  names[iso639_str3_to_key("smn")] = QString("Inari Sami");
  names[iso639_str3_to_key("smo")] = QString("Samoan");
  names[iso639_str3_to_key("sms")] = QString("Skolt Sami");
  names[iso639_str3_to_key("sna")] = QString("Shona");
  names[iso639_str3_to_key("snd")] = QString("Sindhi");
  names[iso639_str3_to_key("snk")] = QString("Soninke");
  names[iso639_str3_to_key("sog")] = QString("Sogdian");
  names[iso639_str3_to_key("som")] = QString("Somali");
  names[iso639_str3_to_key("son")] = QString("Songhai");
  names[iso639_str3_to_key("sot")] = QString("Sotho, Southern");
  names[iso639_str3_to_key("spa")] = QString("Spanish; Castilian");
  names[iso639_str3_to_key("srd")] = QString("Sardinian");
  names[iso639_str3_to_key("srr")] = QString("Serer");
  names[iso639_str3_to_key("ssa")] = QString("Nilo-Saharan (Other)");
  names[iso639_str3_to_key("ssw")] = QString("Swati");
  names[iso639_str3_to_key("suk")] = QString("Sukuma");
  names[iso639_str3_to_key("sun")] = QString("Sundanese");
  names[iso639_str3_to_key("sus")] = QString("Susu");
  names[iso639_str3_to_key("sux")] = QString("Sumerian");
  names[iso639_str3_to_key("swa")] = QString("Swahili");
  names[iso639_str3_to_key("swe")] = QString("Swedish");
  names[iso639_str3_to_key("syr")] = QString("Syriac");
  names[iso639_str3_to_key("tah")] = QString("Tahitian");
  names[iso639_str3_to_key("tai")] = QString("Tai (Other)");
  names[iso639_str3_to_key("tam")] = QString("Tamil");
  names[iso639_str3_to_key("tat")] = QString("Tatar");
  names[iso639_str3_to_key("tel")] = QString("Telugu");
  names[iso639_str3_to_key("tem")] = QString("Timne");
  names[iso639_str3_to_key("ter")] = QString("Tereno");
  names[iso639_str3_to_key("tet")] = QString("Tetum");
  names[iso639_str3_to_key("tgk")] = QString("Tajik");
  names[iso639_str3_to_key("tgl")] = QString("Tagalog");
  names[iso639_str3_to_key("tha")] = QString("Thai");
  names[iso639_str3_to_key("tib")] = QString("Tibetan"); // bod
  names[iso639_str3_to_key("tig")] = QString("Tigre");
  names[iso639_str3_to_key("tir")] = QString("Tigrinya");
  names[iso639_str3_to_key("tiv")] = QString("Tiv");
  names[iso639_str3_to_key("tkl")] = QString("Tokelau");
  names[iso639_str3_to_key("tlh")] = QString("Klingon; tlhlngan-Hol");
  names[iso639_str3_to_key("tli")] = QString("Tlingit");
  names[iso639_str3_to_key("tmh")] = QString("Tamashek");
  names[iso639_str3_to_key("tog")] = QString("Tonga (Nyasa)");
  names[iso639_str3_to_key("ton")] = QString("Tonga (Tonga Islands)");
  names[iso639_str3_to_key("tpi")] = QString("Tok Pisin");
  names[iso639_str3_to_key("tsi")] = QString("Tsimshian");
  names[iso639_str3_to_key("tsn")] = QString("Tswana");
  names[iso639_str3_to_key("tso")] = QString("Tsonga");
  names[iso639_str3_to_key("tuk")] = QString("Turkmen");
  names[iso639_str3_to_key("tum")] = QString("Tumbuka");
  names[iso639_str3_to_key("tup")] = QString("Tupi languages");
  names[iso639_str3_to_key("tur")] = QString("Turkish");
  names[iso639_str3_to_key("tut")] = QString("Altaic (Other)");
  names[iso639_str3_to_key("tvl")] = QString("Tuvalu");
  names[iso639_str3_to_key("twi")] = QString("Twi");
  names[iso639_str3_to_key("tyv")] = QString("Tuvinian");
  names[iso639_str3_to_key("udm")] = QString("Udmurt");
  names[iso639_str3_to_key("uga")] = QString("Ugaritic");
  names[iso639_str3_to_key("uig")] = QString("Uighur");
  names[iso639_str3_to_key("ukr")] = QString("Ukrainian");
  names[iso639_str3_to_key("umb")] = QString("Umbundu");
  names[iso639_str3_to_key("und")] = QString("Undetermined");
  names[iso639_str3_to_key("urd")] = QString("Urdu");
  names[iso639_str3_to_key("uzb")] = QString("Uzbek");
  names[iso639_str3_to_key("vai")] = QString("Vai");
  names[iso639_str3_to_key("ven")] = QString("Venda");
  names[iso639_str3_to_key("vie")] = QString("Vietnamese");
  names[iso639_str3_to_key("vol")] = QString("Volap�k");
  names[iso639_str3_to_key("vot")] = QString("Votic");
  names[iso639_str3_to_key("wak")] = QString("Wakashan languages");
  names[iso639_str3_to_key("wal")] = QString("Walamo");
  names[iso639_str3_to_key("war")] = QString("Waray");
  names[iso639_str3_to_key("was")] = QString("Washo");
  names[iso639_str3_to_key("wel")] = QString("Welsh"); // cym
  names[iso639_str3_to_key("wen")] = QString("Sorbian languages");
  names[iso639_str3_to_key("wln")] = QString("Walloon");
  names[iso639_str3_to_key("wol")] = QString("Wolof");
  names[iso639_str3_to_key("xal")] = QString("Kalmyk");
  names[iso639_str3_to_key("xho")] = QString("Xhosa");
  names[iso639_str3_to_key("yao")] = QString("Yao");
  names[iso639_str3_to_key("yap")] = QString("Yapese");
  names[iso639_str3_to_key("yid")] = QString("Yiddish");
  names[iso639_str3_to_key("yor")] = QString("Yoruba");
  names[iso639_str3_to_key("ypk")] = QString("Yupik languages");
  names[iso639_str3_to_key("zap")] = QString("Zapotec");
  names[iso639_str3_to_key("zen")] = QString("Zenaga");
  names[iso639_str3_to_key("zha")] = QString("Zhuang; Chuang");
  names[iso639_str3_to_key("znd")] = QString("Zande");
  names[iso639_str3_to_key("zul")] = QString("Zulu");
  names[iso639_str3_to_key("zun")] = QString("Zuni");
  return 0;
}

/*
  awk script:
    cat ISO-639-2_values_8bits.txt | \
      awk -F'|' '{if ($3 != "") printf "  codemap[iso639_str2_to_key(\"%s\")] = iso639_str3_to_key(\"%s\");\n", $3, $1}'
*/

static int createCode2ToCode3Map(QMap<int, int>& codemap) {
  codemap[iso639_str2_to_key2("aa")] = iso639_str3_to_key("aar");
  codemap[iso639_str2_to_key2("ab")] = iso639_str3_to_key("abk");
  codemap[iso639_str2_to_key2("af")] = iso639_str3_to_key("afr");
  codemap[iso639_str2_to_key2("ak")] = iso639_str3_to_key("aka");
  codemap[iso639_str2_to_key2("sq")] = iso639_str3_to_key("alb");
  codemap[iso639_str2_to_key2("sq")] = iso639_str3_to_key("alb");
  codemap[iso639_str2_to_key2("am")] = iso639_str3_to_key("amh");
  codemap[iso639_str2_to_key2("ar")] = iso639_str3_to_key("ara");
  codemap[iso639_str2_to_key2("an")] = iso639_str3_to_key("arg");
  codemap[iso639_str2_to_key2("hy")] = iso639_str3_to_key("arm");
  codemap[iso639_str2_to_key2("hy")] = iso639_str3_to_key("arm");
  codemap[iso639_str2_to_key2("as")] = iso639_str3_to_key("asm");
  codemap[iso639_str2_to_key2("av")] = iso639_str3_to_key("ava");
  codemap[iso639_str2_to_key2("ae")] = iso639_str3_to_key("ave");
  codemap[iso639_str2_to_key2("ay")] = iso639_str3_to_key("aym");
  codemap[iso639_str2_to_key2("az")] = iso639_str3_to_key("aze");
  codemap[iso639_str2_to_key2("ba")] = iso639_str3_to_key("bak");
  codemap[iso639_str2_to_key2("bm")] = iso639_str3_to_key("bam");
  codemap[iso639_str2_to_key2("eu")] = iso639_str3_to_key("baq");
  codemap[iso639_str2_to_key2("eu")] = iso639_str3_to_key("baq");
  codemap[iso639_str2_to_key2("be")] = iso639_str3_to_key("bel");
  codemap[iso639_str2_to_key2("bn")] = iso639_str3_to_key("ben");
  codemap[iso639_str2_to_key2("bh")] = iso639_str3_to_key("bih");
  codemap[iso639_str2_to_key2("bi")] = iso639_str3_to_key("bis");
  codemap[iso639_str2_to_key2("bs")] = iso639_str3_to_key("bos");
  codemap[iso639_str2_to_key2("br")] = iso639_str3_to_key("bre");
  codemap[iso639_str2_to_key2("bg")] = iso639_str3_to_key("bul");
  codemap[iso639_str2_to_key2("my")] = iso639_str3_to_key("bur");
  codemap[iso639_str2_to_key2("my")] = iso639_str3_to_key("bur");
  codemap[iso639_str2_to_key2("ca")] = iso639_str3_to_key("cat");
  codemap[iso639_str2_to_key2("ch")] = iso639_str3_to_key("cha");
  codemap[iso639_str2_to_key2("ce")] = iso639_str3_to_key("che");
  codemap[iso639_str2_to_key2("zh")] = iso639_str3_to_key("chi");
  codemap[iso639_str2_to_key2("zh")] = iso639_str3_to_key("chi");
  codemap[iso639_str2_to_key2("cu")] = iso639_str3_to_key("chu");
  codemap[iso639_str2_to_key2("cv")] = iso639_str3_to_key("chv");
  codemap[iso639_str2_to_key2("kw")] = iso639_str3_to_key("cor");
  codemap[iso639_str2_to_key2("co")] = iso639_str3_to_key("cos");
  codemap[iso639_str2_to_key2("cr")] = iso639_str3_to_key("cre");
  codemap[iso639_str2_to_key2("cs")] = iso639_str3_to_key("cze");
  codemap[iso639_str2_to_key2("cs")] = iso639_str3_to_key("cze");
  codemap[iso639_str2_to_key2("da")] = iso639_str3_to_key("dan");
  codemap[iso639_str2_to_key2("dv")] = iso639_str3_to_key("div");
  codemap[iso639_str2_to_key2("nl")] = iso639_str3_to_key("dut");
  codemap[iso639_str2_to_key2("nl")] = iso639_str3_to_key("dut");
  codemap[iso639_str2_to_key2("dz")] = iso639_str3_to_key("dzo");
  codemap[iso639_str2_to_key2("en")] = iso639_str3_to_key("eng");
  codemap[iso639_str2_to_key2("eo")] = iso639_str3_to_key("epo");
  codemap[iso639_str2_to_key2("et")] = iso639_str3_to_key("est");
  codemap[iso639_str2_to_key2("ee")] = iso639_str3_to_key("ewe");
  codemap[iso639_str2_to_key2("fo")] = iso639_str3_to_key("fao");
  codemap[iso639_str2_to_key2("fj")] = iso639_str3_to_key("fij");
  codemap[iso639_str2_to_key2("fi")] = iso639_str3_to_key("fin");
  codemap[iso639_str2_to_key2("fr")] = iso639_str3_to_key("fre");
  codemap[iso639_str2_to_key2("fy")] = iso639_str3_to_key("fry");
  codemap[iso639_str2_to_key2("ff")] = iso639_str3_to_key("ful");
  codemap[iso639_str2_to_key2("ka")] = iso639_str3_to_key("geo");
  codemap[iso639_str2_to_key2("ka")] = iso639_str3_to_key("geo");
  codemap[iso639_str2_to_key2("de")] = iso639_str3_to_key("ger");
  codemap[iso639_str2_to_key2("de")] = iso639_str3_to_key("ger");
  codemap[iso639_str2_to_key2("gd")] = iso639_str3_to_key("gla");
  codemap[iso639_str2_to_key2("ga")] = iso639_str3_to_key("gle");
  codemap[iso639_str2_to_key2("gl")] = iso639_str3_to_key("glg");
  codemap[iso639_str2_to_key2("gv")] = iso639_str3_to_key("glv");
  codemap[iso639_str2_to_key2("el")] = iso639_str3_to_key("gre");
  codemap[iso639_str2_to_key2("el")] = iso639_str3_to_key("gre");
  codemap[iso639_str2_to_key2("gn")] = iso639_str3_to_key("grn");
  codemap[iso639_str2_to_key2("gu")] = iso639_str3_to_key("guj");
  codemap[iso639_str2_to_key2("ht")] = iso639_str3_to_key("hat");
  codemap[iso639_str2_to_key2("ha")] = iso639_str3_to_key("hau");
  codemap[iso639_str2_to_key2("he")] = iso639_str3_to_key("heb");
  codemap[iso639_str2_to_key2("hz")] = iso639_str3_to_key("her");
  codemap[iso639_str2_to_key2("hi")] = iso639_str3_to_key("hin");
  codemap[iso639_str2_to_key2("ho")] = iso639_str3_to_key("hmo");
  codemap[iso639_str2_to_key2("hu")] = iso639_str3_to_key("hun");
  codemap[iso639_str2_to_key2("ig")] = iso639_str3_to_key("ibo");
  codemap[iso639_str2_to_key2("is")] = iso639_str3_to_key("ice");
  codemap[iso639_str2_to_key2("is")] = iso639_str3_to_key("ice");
  codemap[iso639_str2_to_key2("io")] = iso639_str3_to_key("ido");
  codemap[iso639_str2_to_key2("ii")] = iso639_str3_to_key("iii");
  codemap[iso639_str2_to_key2("iu")] = iso639_str3_to_key("iku");
  codemap[iso639_str2_to_key2("ie")] = iso639_str3_to_key("ile");
  codemap[iso639_str2_to_key2("ia")] = iso639_str3_to_key("ina");
  codemap[iso639_str2_to_key2("id")] = iso639_str3_to_key("ind");
  codemap[iso639_str2_to_key2("ik")] = iso639_str3_to_key("ipk");
  codemap[iso639_str2_to_key2("it")] = iso639_str3_to_key("ita");
  codemap[iso639_str2_to_key2("jv")] = iso639_str3_to_key("jav");
  codemap[iso639_str2_to_key2("ja")] = iso639_str3_to_key("jpn");
  codemap[iso639_str2_to_key2("kl")] = iso639_str3_to_key("kal");
  codemap[iso639_str2_to_key2("kn")] = iso639_str3_to_key("kan");
  codemap[iso639_str2_to_key2("ks")] = iso639_str3_to_key("kas");
  codemap[iso639_str2_to_key2("kr")] = iso639_str3_to_key("kau");
  codemap[iso639_str2_to_key2("kk")] = iso639_str3_to_key("kaz");
  codemap[iso639_str2_to_key2("km")] = iso639_str3_to_key("khm");
  codemap[iso639_str2_to_key2("ki")] = iso639_str3_to_key("kik");
  codemap[iso639_str2_to_key2("rw")] = iso639_str3_to_key("kin");
  codemap[iso639_str2_to_key2("ky")] = iso639_str3_to_key("kir");
  codemap[iso639_str2_to_key2("kv")] = iso639_str3_to_key("kom");
  codemap[iso639_str2_to_key2("kg")] = iso639_str3_to_key("kon");
  codemap[iso639_str2_to_key2("ko")] = iso639_str3_to_key("kor");
  codemap[iso639_str2_to_key2("kj")] = iso639_str3_to_key("kua");
  codemap[iso639_str2_to_key2("ku")] = iso639_str3_to_key("kur");
  codemap[iso639_str2_to_key2("lo")] = iso639_str3_to_key("lao");
  codemap[iso639_str2_to_key2("la")] = iso639_str3_to_key("lat");
  codemap[iso639_str2_to_key2("lv")] = iso639_str3_to_key("lav");
  codemap[iso639_str2_to_key2("li")] = iso639_str3_to_key("lim");
  codemap[iso639_str2_to_key2("ln")] = iso639_str3_to_key("lin");
  codemap[iso639_str2_to_key2("lt")] = iso639_str3_to_key("lit");
  codemap[iso639_str2_to_key2("lb")] = iso639_str3_to_key("ltz");
  codemap[iso639_str2_to_key2("lu")] = iso639_str3_to_key("lub");
  codemap[iso639_str2_to_key2("lg")] = iso639_str3_to_key("lug");
  codemap[iso639_str2_to_key2("mk")] = iso639_str3_to_key("mac");
  codemap[iso639_str2_to_key2("mk")] = iso639_str3_to_key("mac");
  codemap[iso639_str2_to_key2("mh")] = iso639_str3_to_key("mah");
  codemap[iso639_str2_to_key2("ml")] = iso639_str3_to_key("mal");
  codemap[iso639_str2_to_key2("mi")] = iso639_str3_to_key("mao");
  codemap[iso639_str2_to_key2("mi")] = iso639_str3_to_key("mao");
  codemap[iso639_str2_to_key2("mr")] = iso639_str3_to_key("mar");
  codemap[iso639_str2_to_key2("ms")] = iso639_str3_to_key("may");
  codemap[iso639_str2_to_key2("ms")] = iso639_str3_to_key("may");
  codemap[iso639_str2_to_key2("mg")] = iso639_str3_to_key("mlg");
  codemap[iso639_str2_to_key2("mt")] = iso639_str3_to_key("mlt");
  codemap[iso639_str2_to_key2("mo")] = iso639_str3_to_key("mol");
  codemap[iso639_str2_to_key2("mn")] = iso639_str3_to_key("mon");
  codemap[iso639_str2_to_key2("na")] = iso639_str3_to_key("nau");
  codemap[iso639_str2_to_key2("nv")] = iso639_str3_to_key("nav");
  codemap[iso639_str2_to_key2("nr")] = iso639_str3_to_key("nbl");
  codemap[iso639_str2_to_key2("nd")] = iso639_str3_to_key("nde");
  codemap[iso639_str2_to_key2("ng")] = iso639_str3_to_key("ndo");
  codemap[iso639_str2_to_key2("ne")] = iso639_str3_to_key("nep");
  codemap[iso639_str2_to_key2("nn")] = iso639_str3_to_key("nno");
  codemap[iso639_str2_to_key2("nb")] = iso639_str3_to_key("nob");
  codemap[iso639_str2_to_key2("no")] = iso639_str3_to_key("nor");
  codemap[iso639_str2_to_key2("ny")] = iso639_str3_to_key("nya");
  codemap[iso639_str2_to_key2("oc")] = iso639_str3_to_key("oci");
  codemap[iso639_str2_to_key2("oj")] = iso639_str3_to_key("oji");
  codemap[iso639_str2_to_key2("or")] = iso639_str3_to_key("ori");
  codemap[iso639_str2_to_key2("om")] = iso639_str3_to_key("orm");
  codemap[iso639_str2_to_key2("os")] = iso639_str3_to_key("oss");
  codemap[iso639_str2_to_key2("pa")] = iso639_str3_to_key("pan");
  codemap[iso639_str2_to_key2("fa")] = iso639_str3_to_key("per");
  codemap[iso639_str2_to_key2("fa")] = iso639_str3_to_key("per");
  codemap[iso639_str2_to_key2("pi")] = iso639_str3_to_key("pli");
  codemap[iso639_str2_to_key2("pl")] = iso639_str3_to_key("pol");
  codemap[iso639_str2_to_key2("pt")] = iso639_str3_to_key("por");
  codemap[iso639_str2_to_key2("ps")] = iso639_str3_to_key("pus");
  codemap[iso639_str2_to_key2("qu")] = iso639_str3_to_key("que");
  codemap[iso639_str2_to_key2("rm")] = iso639_str3_to_key("roh");
  codemap[iso639_str2_to_key2("ro")] = iso639_str3_to_key("rum");
  codemap[iso639_str2_to_key2("rn")] = iso639_str3_to_key("run");
  codemap[iso639_str2_to_key2("ru")] = iso639_str3_to_key("rus");
  codemap[iso639_str2_to_key2("sg")] = iso639_str3_to_key("sag");
  codemap[iso639_str2_to_key2("sa")] = iso639_str3_to_key("san");
  codemap[iso639_str2_to_key2("sr")] = iso639_str3_to_key("scc");
  codemap[iso639_str2_to_key2("sr")] = iso639_str3_to_key("scc");
  codemap[iso639_str2_to_key2("hr")] = iso639_str3_to_key("scr");
  codemap[iso639_str2_to_key2("hr")] = iso639_str3_to_key("scr");
  codemap[iso639_str2_to_key2("si")] = iso639_str3_to_key("sin");
  codemap[iso639_str2_to_key2("sk")] = iso639_str3_to_key("slo");
  codemap[iso639_str2_to_key2("sl")] = iso639_str3_to_key("slv");
  codemap[iso639_str2_to_key2("se")] = iso639_str3_to_key("sme");
  codemap[iso639_str2_to_key2("sm")] = iso639_str3_to_key("smo");
  codemap[iso639_str2_to_key2("sn")] = iso639_str3_to_key("sna");
  codemap[iso639_str2_to_key2("sd")] = iso639_str3_to_key("snd");
  codemap[iso639_str2_to_key2("so")] = iso639_str3_to_key("som");
  codemap[iso639_str2_to_key2("st")] = iso639_str3_to_key("sot");
  codemap[iso639_str2_to_key2("es")] = iso639_str3_to_key("spa");
  codemap[iso639_str2_to_key2("sc")] = iso639_str3_to_key("srd");
  codemap[iso639_str2_to_key2("ss")] = iso639_str3_to_key("ssw");
  codemap[iso639_str2_to_key2("su")] = iso639_str3_to_key("sun");
  codemap[iso639_str2_to_key2("sw")] = iso639_str3_to_key("swa");
  codemap[iso639_str2_to_key2("sv")] = iso639_str3_to_key("swe");
  codemap[iso639_str2_to_key2("ty")] = iso639_str3_to_key("tah");
  codemap[iso639_str2_to_key2("ta")] = iso639_str3_to_key("tam");
  codemap[iso639_str2_to_key2("tt")] = iso639_str3_to_key("tat");
  codemap[iso639_str2_to_key2("te")] = iso639_str3_to_key("tel");
  codemap[iso639_str2_to_key2("tg")] = iso639_str3_to_key("tgk");
  codemap[iso639_str2_to_key2("tl")] = iso639_str3_to_key("tgl");
  codemap[iso639_str2_to_key2("th")] = iso639_str3_to_key("tha");
  codemap[iso639_str2_to_key2("bo")] = iso639_str3_to_key("tib");
  codemap[iso639_str2_to_key2("bo")] = iso639_str3_to_key("tib");
  codemap[iso639_str2_to_key2("ti")] = iso639_str3_to_key("tir");
  codemap[iso639_str2_to_key2("to")] = iso639_str3_to_key("ton");
  codemap[iso639_str2_to_key2("tn")] = iso639_str3_to_key("tsn");
  codemap[iso639_str2_to_key2("ts")] = iso639_str3_to_key("tso");
  codemap[iso639_str2_to_key2("tk")] = iso639_str3_to_key("tuk");
  codemap[iso639_str2_to_key2("tr")] = iso639_str3_to_key("tur");
  codemap[iso639_str2_to_key2("tw")] = iso639_str3_to_key("twi");
  codemap[iso639_str2_to_key2("ug")] = iso639_str3_to_key("uig");
  codemap[iso639_str2_to_key2("uk")] = iso639_str3_to_key("ukr");
  codemap[iso639_str2_to_key2("ur")] = iso639_str3_to_key("urd");
  codemap[iso639_str2_to_key2("uz")] = iso639_str3_to_key("uzb");
  codemap[iso639_str2_to_key2("ve")] = iso639_str3_to_key("ven");
  codemap[iso639_str2_to_key2("vi")] = iso639_str3_to_key("vie");
  codemap[iso639_str2_to_key2("vo")] = iso639_str3_to_key("vol");
  codemap[iso639_str2_to_key2("cy")] = iso639_str3_to_key("wel");
  codemap[iso639_str2_to_key2("cy")] = iso639_str3_to_key("wel");
  codemap[iso639_str2_to_key2("wa")] = iso639_str3_to_key("wln");
  codemap[iso639_str2_to_key2("wo")] = iso639_str3_to_key("wol");
  codemap[iso639_str2_to_key2("xh")] = iso639_str3_to_key("xho");
  codemap[iso639_str2_to_key2("yi")] = iso639_str3_to_key("yid");
  codemap[iso639_str2_to_key2("yo")] = iso639_str3_to_key("yor");
  codemap[iso639_str2_to_key2("za")] = iso639_str3_to_key("zha");
  codemap[iso639_str2_to_key2("zu")] = iso639_str3_to_key("zul");
  return 0;
}

/*
    The following has yet to be integrated with the preceeding code in a
    meaningful way but it is stored here because it provides ISO639 related
    functionality.
*/

typedef QMap<QString, QString> ISO639ToNameMap;
static ISO639ToNameMap createLanguageMap(void)
{
    ISO639ToNameMap map;
    map["af"] = QString::fromUtf8("Afrikaans");
    map["am"] = QString::fromUtf8("አማርኛ");
    map["ar"] = QString::fromUtf8("العربية");
    map["as"] = QString::fromUtf8("অসমীয়া");
    map["az"] = QString::fromUtf8("Azərbaycan türkçəsi");
    map["be"] = QString::fromUtf8("Беларуская");
    map["bg"] = QString::fromUtf8("Български");
    map["bn"] = QString::fromUtf8("বাংলা");
    map["br"] = QString::fromUtf8("Brezhoneg");
    map["bs"] = QString::fromUtf8("Rumunjki");
    map["ca"] = QString::fromUtf8("català; valencià");
    map["cs"] = QString::fromUtf8("čeština");
    map["cy"] = QString::fromUtf8("Cymraeg");
    map["da"] = QString::fromUtf8("Dansk");
    map["de"] = QString::fromUtf8("Deutsch");
    map["el"] = QString::fromUtf8("Ελληνικά, Σύγχρονα");
    map["en"] = QString::fromUtf8("English");
    map["eo"] = QString::fromUtf8("Esperanto");
    map["es"] = QString::fromUtf8("Español; Castellano");
    map["et"] = QString::fromUtf8("Eesti");
    map["eu"] = QString::fromUtf8("Euskara");
    map["fa"] = QString::fromUtf8("فارسی");
    map["fi"] = QString::fromUtf8("suomi");
    map["fr"] = QString::fromUtf8("Français");
    map["ga"] = QString::fromUtf8("Gaeilge");
    map["gl"] = QString::fromUtf8("Galego");
    map["gu"] = QString::fromUtf8("ગુજરાતી");
    map["he"] = QString::fromUtf8("עברית");
    map["hi"] = QString::fromUtf8("हिंदी");
    map["hr"] = QString::fromUtf8("Hrvatski");
    map["hu"] = QString::fromUtf8("magyar");
    map["id"] = QString::fromUtf8("Bahasa Indonesia");
    map["is"] = QString::fromUtf8("Íslenska");
    map["it"] = QString::fromUtf8("Italiano");
    map["ja"] = QString::fromUtf8("日本語");
    map["kn"] = QString::fromUtf8("ಕನ್ನಡ");
    map["ko"] = QString::fromUtf8("한국어");
    map["lt"] = QString::fromUtf8("Lietuvių");
    map["lv"] = QString::fromUtf8("Latviešu");
    map["mi"] = QString::fromUtf8("Reo Māori");
    map["mk"] = QString::fromUtf8("Македонски");
    map["ml"] = QString::fromUtf8("മലയാളം");
    map["mn"] = QString::fromUtf8("Монгол");
    map["mr"] = QString::fromUtf8("मराठी");
    map["ms"] = QString::fromUtf8("Bahasa Melayu");
    map["mt"] = QString::fromUtf8("Malti");
    map["nb"] = QString::fromUtf8("Norsk, bokmål");
    map["nl"] = QString::fromUtf8("Nederlands");
    map["nn"] = QString::fromUtf8("Norsk (nynorsk)");
    map["oc"] = QString::fromUtf8("Occitan (aprèp 1500)");
    map["or"] = QString::fromUtf8("ଓଡିଆ");
    map["pa"] = QString::fromUtf8("ਪੰਜਾਬੀ");
    map["pl"] = QString::fromUtf8("polski");
    map["pt"] = QString::fromUtf8("Português");
    map["ro"] = QString::fromUtf8("Română");
    map["ru"] = QString::fromUtf8("русский");
    map["rw"] = QString::fromUtf8("Ikinyarwanda");
    map["sk"] = QString::fromUtf8("slovenčina");
    map["sl"] = QString::fromUtf8("slovenščina");
    map["sr"] = QString::fromUtf8("српски");
    map["sv"] = QString::fromUtf8("Svenska");
    map["ta"] = QString::fromUtf8("தமிழ்");
    map["te"] = QString::fromUtf8("తెలుగు");
    map["th"] = QString::fromUtf8("ไทย");
    map["ti"] = QString::fromUtf8("ትግርኛ");
    map["tr"] = QString::fromUtf8("Türkçe");
    map["tt"] = QString::fromUtf8("Tatarça");
    map["uk"] = QString::fromUtf8("українська");
    map["ve"] = QString::fromUtf8("Venda");
    map["vi"] = QString::fromUtf8("Tiếng Việt");
    map["wa"] = QString::fromUtf8("Walon");
    map["xh"] = QString::fromUtf8("isiXhosa");
    map["zh"] = QString::fromUtf8("漢語");
    map["zu"] = QString::fromUtf8("Isi-Zulu");
    return map;
}

static ISO639ToNameMap gLanguageMap;

QString GetISO639LanguageName(QString iso639_1)
{
    if (gLanguageMap.isEmpty())
        gLanguageMap = createLanguageMap();

    return gLanguageMap[iso639_1];
}

QString GetISO639EnglishLanguageName(QString iso639_1)
{
    QString iso639_2 = iso639_str2_to_str3(iso639_1);
    int key2 = iso639_str3_to_key(iso639_2);
    return iso639_key_toName(key2);
}
