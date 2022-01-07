# -*- coding: utf-8 -*-
#-----------------------
# Name: locales.py    Stores locale information for filtering results
# Python Library
# Author: Raymond Wagner
#-----------------------

from .tmdb_exceptions import *
import locale

syslocale = None


class LocaleBase(object):
    __slots__ = ['__immutable']
    _stored = {}
    fallthrough = False

    def __init__(self, *keys):
        for key in keys:
            self._stored[key.lower()] = self
        self.__immutable = True

    def __setattr__(self, key, value):
        if getattr(self, '__immutable', False):
            raise NotImplementedError(self.__class__.__name__ +
                                      ' does not support modification.')
        super(LocaleBase, self).__setattr__(key, value)

    def __delattr__(self, key):
        if getattr(self, '__immutable', False):
            raise NotImplementedError(self.__class__.__name__ +
                                      ' does not support modification.')
        super(LocaleBase, self).__delattr__(key)

    def __lt__(self, other):
        return (id(self) != id(other)) and (str(self) > str(other))

    def __gt__(self, other):
        return (id(self) != id(other)) and (str(self) < str(other))

    def __eq__(self, other):
        return (id(self) == id(other)) or (str(self) == str(other))

    @classmethod
    def getstored(cls, key):
        if key is None:
            return None
        try:
            return cls._stored[key.lower()]
        except:
            raise TMDBLocaleError("'{0}' is not a known valid {1} code."\
                                  .format(key, cls.__name__))


class Language(LocaleBase):
    __slots__ = ['ISO639_1', 'ISO639_2', 'ISO639_2B', 'englishname',
                 'nativename']
    _stored = {}

    def __init__(self, iso1, iso2, ename):
        self.ISO639_1 = iso1
        self.ISO639_2 = iso2
#        self.ISO639_2B = iso2b
        self.englishname = ename
#        self.nativename = nname
        super(Language, self).__init__(iso1, iso2)

    def __str__(self):
        return self.ISO639_1

    def __repr__(self):
        return "<Language '{0.englishname}' ({0.ISO639_1})>".format(self)


class Country(LocaleBase):
    __slots__ = ['alpha2', 'name']
    _stored = {}

    def __init__(self, alpha2, name):
        self.alpha2 = alpha2
        self.name = name
        super(Country, self).__init__(alpha2)

    def __str__(self):
        return self.alpha2

    def __repr__(self):
        return "<Country '{0.name}' ({0.alpha2})>".format(self)


class Locale(LocaleBase):
    __slots__ = ['language', 'country', 'encoding']

    def __init__(self, language, country, encoding):
        self.language = Language.getstored(language)
        self.country = Country.getstored(country)
        self.encoding = encoding if encoding else 'latin-1'

    def __str__(self):
        return "{0}_{1}".format(self.language, self.country)

    def __repr__(self):
        return "<Locale {0.language}_{0.country}>".format(self)

    def encode(self, dat):
        """Encode using system default encoding for network/file output."""
        try:
            return dat.encode(self.encoding)
        except AttributeError:
            # not a string type, pass along
            return dat
        except UnicodeDecodeError:
            # just return unmodified and hope for the best
            return dat

    def decode(self, dat):
        """Decode to system default encoding for internal use."""
        try:
            return dat.decode(self.encoding)
        except AttributeError:
            # not a string type, pass along
            return dat
        except UnicodeEncodeError:
            # just return unmodified and hope for the best
            return dat


def set_locale(language=None, country=None, fallthrough=False):
    global syslocale
    LocaleBase.fallthrough = fallthrough

    sysloc, sysenc = locale.getdefaultlocale()

    if (not language) or (not country):
        dat = None
        if syslocale is not None:
            dat = (str(syslocale.language), str(syslocale.country))
        else:
            if (sysloc is None) or ('_' not in sysloc):
                dat = ('en', 'US')
            else:
                dat = sysloc.split('_')
        if language is None:
            language = dat[0]
        if country is None:
            country = dat[1]

    syslocale = Locale(language, country, sysenc)


def get_locale(language=-1, country=-1):
    """Output locale using provided attributes, or return system locale."""
    global syslocale
    # pull existing stored values
    if syslocale is None:
        loc = Locale(None, None, locale.getdefaultlocale()[1])
    else:
        loc = syslocale

    # both options are default, return stored values
    if language == country == -1:
        return loc

    # supplement default option with stored values
    if language == -1:
        language = loc.language
    elif country == -1:
        country = loc.country
    return Locale(language, country, loc.encoding)

######## AUTOGENERATED LANGUAGE AND COUNTRY DATA BELOW HERE #########

Language("ab", "abk", "Abkhazian")
Language("aa", "aar", "Afar")
Language("af", "afr", "Afrikaans")
Language("ak", "aka", "Akan")
Language("sq", "alb/sqi", "Albanian")
Language("am", "amh", "Amharic")
Language("ar", "ara", "Arabic")
Language("an", "arg", "Aragonese")
Language("hy", "arm/hye", "Armenian")
Language("as", "asm", "Assamese")
Language("av", "ava", "Avaric")
Language("ae", "ave", "Avestan")
Language("ay", "aym", "Aymara")
Language("az", "aze", "Azerbaijani")
Language("bm", "bam", "Bambara")
Language("ba", "bak", "Bashkir")
Language("eu", "baq/eus", "Basque")
Language("be", "bel", "Belarusian")
Language("bn", "ben", "Bengali")
Language("bh", "bih", "Bihari languages")
Language("bi", "bis", "Bislama")
Language("nb", "nob", "Bokmål, Norwegian")
Language("bs", "bos", "Bosnian")
Language("br", "bre", "Breton")
Language("bg", "bul", "Bulgarian")
Language("my", "bur/mya", "Burmese")
Language("es", "spa", "Castilian")
Language("ca", "cat", "Catalan")
Language("km", "khm", "Central Khmer")
Language("ch", "cha", "Chamorro")
Language("ce", "che", "Chechen")
Language("ny", "nya", "Chewa")
Language("ny", "nya", "Chichewa")
Language("zh", "chi/zho", "Chinese")
Language("za", "zha", "Chuang")
Language("cu", "chu", "Church Slavic")
Language("cu", "chu", "Church Slavonic")
Language("cv", "chv", "Chuvash")
Language("kw", "cor", "Cornish")
Language("co", "cos", "Corsican")
Language("cr", "cre", "Cree")
Language("hr", "hrv", "Croatian")
Language("cs", "cze/ces", "Czech")
Language("da", "dan", "Danish")
Language("dv", "div", "Dhivehi")
Language("dv", "div", "Divehi")
Language("nl", "dut/nld", "Dutch")
Language("dz", "dzo", "Dzongkha")
Language("en", "eng", "English")
Language("eo", "epo", "Esperanto")
Language("et", "est", "Estonian")
Language("ee", "ewe", "Ewe")
Language("fo", "fao", "Faroese")
Language("fj", "fij", "Fijian")
Language("fi", "fin", "Finnish")
Language("nl", "dut/nld", "Flemish")
Language("fr", "fre/fra", "French")
Language("ff", "ful", "Fulah")
Language("gd", "gla", "Gaelic")
Language("gl", "glg", "Galician")
Language("lg", "lug", "Ganda")
Language("ka", "geo/kat", "Georgian")
Language("de", "ger/deu", "German")
Language("ki", "kik", "Gikuyu")
Language("el", "gre/ell", "Greek, Modern (1453-)")
Language("kl", "kal", "Greenlandic")
Language("gn", "grn", "Guarani")
Language("gu", "guj", "Gujarati")
Language("ht", "hat", "Haitian")
Language("ht", "hat", "Haitian Creole")
Language("ha", "hau", "Hausa")
Language("he", "heb", "Hebrew")
Language("hz", "her", "Herero")
Language("hi", "hin", "Hindi")
Language("ho", "hmo", "Hiri Motu")
Language("hu", "hun", "Hungarian")
Language("is", "ice/isl", "Icelandic")
Language("io", "ido", "Ido")
Language("ig", "ibo", "Igbo")
Language("id", "ind", "Indonesian")
Language("ia", "ina", "Interlingua (International Auxiliary Language Association)")
Language("ie", "ile", "Interlingue")
Language("iu", "iku", "Inuktitut")
Language("ik", "ipk", "Inupiaq")
Language("ga", "gle", "Irish")
Language("it", "ita", "Italian")
Language("ja", "jpn", "Japanese")
Language("jv", "jav", "Javanese")
Language("kl", "kal", "Kalaallisut")
Language("kn", "kan", "Kannada")
Language("kr", "kau", "Kanuri")
Language("ks", "kas", "Kashmiri")
Language("kk", "kaz", "Kazakh")
Language("ki", "kik", "Kikuyu")
Language("rw", "kin", "Kinyarwanda")
Language("ky", "kir", "Kirghiz")
Language("kv", "kom", "Komi")
Language("kg", "kon", "Kongo")
Language("ko", "kor", "Korean")
Language("kj", "kua", "Kuanyama")
Language("ku", "kur", "Kurdish")
Language("kj", "kua", "Kwanyama")
Language("ky", "kir", "Kyrgyz")
Language("lo", "lao", "Lao")
Language("la", "lat", "Latin")
Language("lv", "lav", "Latvian")
Language("lb", "ltz", "Letzeburgesch")
Language("li", "lim", "Limburgan")
Language("li", "lim", "Limburger")
Language("li", "lim", "Limburgish")
Language("ln", "lin", "Lingala")
Language("lt", "lit", "Lithuanian")
Language("lu", "lub", "Luba-Katanga")
Language("lb", "ltz", "Luxembourgish")
Language("mk", "mac/mkd", "Macedonian")
Language("mg", "mlg", "Malagasy")
Language("ms", "may/msa", "Malay")
Language("ml", "mal", "Malayalam")
Language("dv", "div", "Maldivian")
Language("mt", "mlt", "Maltese")
Language("gv", "glv", "Manx")
Language("mi", "mao/mri", "Maori")
Language("mr", "mar", "Marathi")
Language("mh", "mah", "Marshallese")
Language("ro", "rum/ron", "Moldavian")
Language("ro", "rum/ron", "Moldovan")
Language("mn", "mon", "Mongolian")
Language("na", "nau", "Nauru")
Language("nv", "nav", "Navaho")
Language("nv", "nav", "Navajo")
Language("nd", "nde", "Ndebele, North")
Language("nr", "nbl", "Ndebele, South")
Language("ng", "ndo", "Ndonga")
Language("ne", "nep", "Nepali")
Language("nd", "nde", "North Ndebele")
Language("se", "sme", "Northern Sami")
Language("no", "nor", "Norwegian")
Language("nb", "nob", "Norwegian Bokmål")
Language("nn", "nno", "Norwegian Nynorsk")
Language("ii", "iii", "Nuosu")
Language("ny", "nya", "Nyanja")
Language("nn", "nno", "Nynorsk, Norwegian")
Language("ie", "ile", "Occidental")
Language("oc", "oci", "Occitan (post 1500)")
Language("oj", "oji", "Ojibwa")
Language("cu", "chu", "Old Bulgarian")
Language("cu", "chu", "Old Church Slavonic")
Language("cu", "chu", "Old Slavonic")
Language("or", "ori", "Oriya")
Language("om", "orm", "Oromo")
Language("os", "oss", "Ossetian")
Language("os", "oss", "Ossetic")
Language("pi", "pli", "Pali")
Language("pa", "pan", "Panjabi")
Language("ps", "pus", "Pashto")
Language("fa", "per/fas", "Persian")
Language("pl", "pol", "Polish")
Language("pt", "por", "Portuguese")
Language("pa", "pan", "Punjabi")
Language("ps", "pus", "Pushto")
Language("qu", "que", "Quechua")
Language("ro", "rum/ron", "Romanian")
Language("rm", "roh", "Romansh")
Language("rn", "run", "Rundi")
Language("ru", "rus", "Russian")
Language("sm", "smo", "Samoan")
Language("sg", "sag", "Sango")
Language("sa", "san", "Sanskrit")
Language("sc", "srd", "Sardinian")
Language("gd", "gla", "Scottish Gaelic")
Language("sr", "srp", "Serbian")
Language("sn", "sna", "Shona")
Language("ii", "iii", "Sichuan Yi")
Language("sd", "snd", "Sindhi")
Language("si", "sin", "Sinhala")
Language("si", "sin", "Sinhalese")
Language("sk", "slo/slk", "Slovak")
Language("sl", "slv", "Slovenian")
Language("so", "som", "Somali")
Language("st", "sot", "Sotho, Southern")
Language("nr", "nbl", "South Ndebele")
Language("es", "spa", "Spanish")
Language("su", "sun", "Sundanese")
Language("sw", "swa", "Swahili")
Language("ss", "ssw", "Swati")
Language("sv", "swe", "Swedish")
Language("tl", "tgl", "Tagalog")
Language("ty", "tah", "Tahitian")
Language("tg", "tgk", "Tajik")
Language("ta", "tam", "Tamil")
Language("tt", "tat", "Tatar")
Language("te", "tel", "Telugu")
Language("th", "tha", "Thai")
Language("bo", "tib/bod", "Tibetan")
Language("ti", "tir", "Tigrinya")
Language("to", "ton", "Tonga (Tonga Islands)")
Language("ts", "tso", "Tsonga")
Language("tn", "tsn", "Tswana")
Language("tr", "tur", "Turkish")
Language("tk", "tuk", "Turkmen")
Language("tw", "twi", "Twi")
Language("ug", "uig", "Uighur")
Language("uk", "ukr", "Ukrainian")
Language("ur", "urd", "Urdu")
Language("ug", "uig", "Uyghur")
Language("uz", "uzb", "Uzbek")
Language("ca", "cat", "Valencian")
Language("ve", "ven", "Venda")
Language("vi", "vie", "Vietnamese")
Language("vo", "vol", "Volapük")
Language("wa", "wln", "Walloon")
Language("cy", "wel/cym", "Welsh")
Language("fy", "fry", "Western Frisian")
Language("wo", "wol", "Wolof")
Language("xh", "xho", "Xhosa")
Language("yi", "yid", "Yiddish")
Language("yo", "yor", "Yoruba")
Language("za", "zha", "Zhuang")
Language("zu", "zul", "Zulu")
Country("AF", "AFGHANISTAN")
Country("AX", "ÅLAND ISLANDS")
Country("AL", "ALBANIA")
Country("DZ", "ALGERIA")
Country("AS", "AMERICAN SAMOA")
Country("AD", "ANDORRA")
Country("AO", "ANGOLA")
Country("AI", "ANGUILLA")
Country("AQ", "ANTARCTICA")
Country("AG", "ANTIGUA AND BARBUDA")
Country("AR", "ARGENTINA")
Country("AM", "ARMENIA")
Country("AW", "ARUBA")
Country("AU", "AUSTRALIA")
Country("AT", "AUSTRIA")
Country("AZ", "AZERBAIJAN")
Country("BS", "BAHAMAS")
Country("BH", "BAHRAIN")
Country("BD", "BANGLADESH")
Country("BB", "BARBADOS")
Country("BY", "BELARUS")
Country("BE", "BELGIUM")
Country("BZ", "BELIZE")
Country("BJ", "BENIN")
Country("BM", "BERMUDA")
Country("BT", "BHUTAN")
Country("BO", "BOLIVIA, PLURINATIONAL STATE OF")
Country("BQ", "BONAIRE, SINT EUSTATIUS AND SABA")
Country("BA", "BOSNIA AND HERZEGOVINA")
Country("BW", "BOTSWANA")
Country("BV", "BOUVET ISLAND")
Country("BR", "BRAZIL")
Country("IO", "BRITISH INDIAN OCEAN TERRITORY")
Country("BN", "BRUNEI DARUSSALAM")
Country("BG", "BULGARIA")
Country("BF", "BURKINA FASO")
Country("BI", "BURUNDI")
Country("KH", "CAMBODIA")
Country("CM", "CAMEROON")
Country("CA", "CANADA")
Country("CV", "CAPE VERDE")
Country("KY", "CAYMAN ISLANDS")
Country("CF", "CENTRAL AFRICAN REPUBLIC")
Country("TD", "CHAD")
Country("CL", "CHILE")
Country("CN", "CHINA")
Country("CX", "CHRISTMAS ISLAND")
Country("CC", "COCOS (KEELING) ISLANDS")
Country("CO", "COLOMBIA")
Country("KM", "COMOROS")
Country("CG", "CONGO")
Country("CD", "CONGO, THE DEMOCRATIC REPUBLIC OF THE")
Country("CK", "COOK ISLANDS")
Country("CR", "COSTA RICA")
Country("CI", "CÔTE D'IVOIRE")
Country("HR", "CROATIA")
Country("CU", "CUBA")
Country("CW", "CURAÇAO")
Country("CY", "CYPRUS")
Country("CZ", "CZECH REPUBLIC")
Country("DK", "DENMARK")
Country("DJ", "DJIBOUTI")
Country("DM", "DOMINICA")
Country("DO", "DOMINICAN REPUBLIC")
Country("EC", "ECUADOR")
Country("EG", "EGYPT")
Country("SV", "EL SALVADOR")
Country("GQ", "EQUATORIAL GUINEA")
Country("ER", "ERITREA")
Country("EE", "ESTONIA")
Country("ET", "ETHIOPIA")
Country("FK", "FALKLAND ISLANDS (MALVINAS)")
Country("FO", "FAROE ISLANDS")
Country("FJ", "FIJI")
Country("FI", "FINLAND")
Country("FR", "FRANCE")
Country("GF", "FRENCH GUIANA")
Country("PF", "FRENCH POLYNESIA")
Country("TF", "FRENCH SOUTHERN TERRITORIES")
Country("GA", "GABON")
Country("GM", "GAMBIA")
Country("GE", "GEORGIA")
Country("DE", "GERMANY")
Country("GH", "GHANA")
Country("GI", "GIBRALTAR")
Country("GR", "GREECE")
Country("GL", "GREENLAND")
Country("GD", "GRENADA")
Country("GP", "GUADELOUPE")
Country("GU", "GUAM")
Country("GT", "GUATEMALA")
Country("GG", "GUERNSEY")
Country("GN", "GUINEA")
Country("GW", "GUINEA-BISSAU")
Country("GY", "GUYANA")
Country("HT", "HAITI")
Country("HM", "HEARD ISLAND AND MCDONALD ISLANDS")
Country("VA", "HOLY SEE (VATICAN CITY STATE)")
Country("HN", "HONDURAS")
Country("HK", "HONG KONG")
Country("HU", "HUNGARY")
Country("IS", "ICELAND")
Country("IN", "INDIA")
Country("ID", "INDONESIA")
Country("IR", "IRAN, ISLAMIC REPUBLIC OF")
Country("IQ", "IRAQ")
Country("IE", "IRELAND")
Country("IM", "ISLE OF MAN")
Country("IL", "ISRAEL")
Country("IT", "ITALY")
Country("JM", "JAMAICA")
Country("JP", "JAPAN")
Country("JE", "JERSEY")
Country("JO", "JORDAN")
Country("KZ", "KAZAKHSTAN")
Country("KE", "KENYA")
Country("KI", "KIRIBATI")
Country("KP", "KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF")
Country("KR", "KOREA, REPUBLIC OF")
Country("KW", "KUWAIT")
Country("KG", "KYRGYZSTAN")
Country("LA", "LAO PEOPLE'S DEMOCRATIC REPUBLIC")
Country("LV", "LATVIA")
Country("LB", "LEBANON")
Country("LS", "LESOTHO")
Country("LR", "LIBERIA")
Country("LY", "LIBYA")
Country("LI", "LIECHTENSTEIN")
Country("LT", "LITHUANIA")
Country("LU", "LUXEMBOURG")
Country("MO", "MACAO")
Country("MK", "MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF")
Country("MG", "MADAGASCAR")
Country("MW", "MALAWI")
Country("MY", "MALAYSIA")
Country("MV", "MALDIVES")
Country("ML", "MALI")
Country("MT", "MALTA")
Country("MH", "MARSHALL ISLANDS")
Country("MQ", "MARTINIQUE")
Country("MR", "MAURITANIA")
Country("MU", "MAURITIUS")
Country("YT", "MAYOTTE")
Country("MX", "MEXICO")
Country("FM", "MICRONESIA, FEDERATED STATES OF")
Country("MD", "MOLDOVA, REPUBLIC OF")
Country("MC", "MONACO")
Country("MN", "MONGOLIA")
Country("ME", "MONTENEGRO")
Country("MS", "MONTSERRAT")
Country("MA", "MOROCCO")
Country("MZ", "MOZAMBIQUE")
Country("MM", "MYANMAR")
Country("NA", "NAMIBIA")
Country("NR", "NAURU")
Country("NP", "NEPAL")
Country("NL", "NETHERLANDS")
Country("NC", "NEW CALEDONIA")
Country("NZ", "NEW ZEALAND")
Country("NI", "NICARAGUA")
Country("NE", "NIGER")
Country("NG", "NIGERIA")
Country("NU", "NIUE")
Country("NF", "NORFOLK ISLAND")
Country("MP", "NORTHERN MARIANA ISLANDS")
Country("NO", "NORWAY")
Country("OM", "OMAN")
Country("PK", "PAKISTAN")
Country("PW", "PALAU")
Country("PS", "PALESTINIAN TERRITORY, OCCUPIED")
Country("PA", "PANAMA")
Country("PG", "PAPUA NEW GUINEA")
Country("PY", "PARAGUAY")
Country("PE", "PERU")
Country("PH", "PHILIPPINES")
Country("PN", "PITCAIRN")
Country("PL", "POLAND")
Country("PT", "PORTUGAL")
Country("PR", "PUERTO RICO")
Country("QA", "QATAR")
Country("RE", "RÉUNION")
Country("RO", "ROMANIA")
Country("RU", "RUSSIAN FEDERATION")
Country("RW", "RWANDA")
Country("BL", "SAINT BARTHÉLEMY")
Country("SH", "SAINT HELENA, ASCENSION AND TRISTAN DA CUNHA")
Country("KN", "SAINT KITTS AND NEVIS")
Country("LC", "SAINT LUCIA")
Country("MF", "SAINT MARTIN (FRENCH PART)")
Country("PM", "SAINT PIERRE AND MIQUELON")
Country("VC", "SAINT VINCENT AND THE GRENADINES")
Country("WS", "SAMOA")
Country("SM", "SAN MARINO")
Country("ST", "SAO TOME AND PRINCIPE")
Country("SA", "SAUDI ARABIA")
Country("SN", "SENEGAL")
Country("RS", "SERBIA")
Country("SC", "SEYCHELLES")
Country("SL", "SIERRA LEONE")
Country("SG", "SINGAPORE")
Country("SX", "SINT MAARTEN (DUTCH PART)")
Country("SK", "SLOVAKIA")
Country("SI", "SLOVENIA")
Country("SB", "SOLOMON ISLANDS")
Country("SO", "SOMALIA")
Country("ZA", "SOUTH AFRICA")
Country("GS", "SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS")
Country("SS", "SOUTH SUDAN")
Country("ES", "SPAIN")
Country("LK", "SRI LANKA")
Country("SD", "SUDAN")
Country("SR", "SURINAME")
Country("SJ", "SVALBARD AND JAN MAYEN")
Country("SZ", "SWAZILAND")
Country("SE", "SWEDEN")
Country("CH", "SWITZERLAND")
Country("SY", "SYRIAN ARAB REPUBLIC")
Country("TW", "TAIWAN, PROVINCE OF CHINA")
Country("TJ", "TAJIKISTAN")
Country("TZ", "TANZANIA, UNITED REPUBLIC OF")
Country("TH", "THAILAND")
Country("TL", "TIMOR-LESTE")
Country("TG", "TOGO")
Country("TK", "TOKELAU")
Country("TO", "TONGA")
Country("TT", "TRINIDAD AND TOBAGO")
Country("TN", "TUNISIA")
Country("TR", "TURKEY")
Country("TM", "TURKMENISTAN")
Country("TC", "TURKS AND CAICOS ISLANDS")
Country("TV", "TUVALU")
Country("UG", "UGANDA")
Country("UA", "UKRAINE")
Country("AE", "UNITED ARAB EMIRATES")
Country("GB", "UNITED KINGDOM")
Country("US", "UNITED STATES")
Country("UM", "UNITED STATES MINOR OUTLYING ISLANDS")
Country("UY", "URUGUAY")
Country("UZ", "UZBEKISTAN")
Country("VU", "VANUATU")
Country("VE", "VENEZUELA, BOLIVARIAN REPUBLIC OF")
Country("VN", "VIET NAM")
Country("VG", "VIRGIN ISLANDS, BRITISH")
Country("VI", "VIRGIN ISLANDS, U.S.")
Country("WF", "WALLIS AND FUTUNA")
Country("EH", "WESTERN SAHARA")
Country("YE", "YEMEN")
Country("ZM", "ZAMBIA")
Country("ZW", "ZIMBABWE")
