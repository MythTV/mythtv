# -*- coding: utf-8 -*-

#-----------------------
# Name: locales.py    Stores locale information for filtering results
# Python Library
# Author: Raymond Wagner
#-----------------------
# Author: Roland Ernst
# Modifications:
#   - Added ISO931-2T/2B
#   - Added ISO-3166 alpha-3
#   - Removed unused code
#----------------------

import locale


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

    @classmethod
    def getstored(cls, key):
        if key is None:
            return None
        try:
            return cls._stored[key.lower()]
        except:
            raise Exception("'{0}' is not a known valid {1} code."\
                                  .format(key, cls.__name__))


class Language(LocaleBase):
    __slots__ = ['ISO639_1', 'ISO639_2T', 'ISO639_2B', 'englishname',
                 'nativename']
    _stored = {}

    def __init__(self, iso1, iso2b, iso2t, ename):
        self.ISO639_1 = iso1
        self.ISO639_2B = iso2b
        self.ISO639_2T = iso2t
        self.englishname = ename
#        self.nativename = nname
        super(Language, self).__init__(iso1, iso2b, iso2t, ename)

    def __str__(self):
        return self.ISO639_1

    def __repr__(self):
        return u"<Language '{0.englishname}' ({0.ISO639_1}) ({0.ISO639_2T}) ({0.ISO639_2B})>".format(self)


class Country(LocaleBase):
    __slots__ = ['alpha2', 'alpha3', 'name']
    _stored = {}

    def __init__(self, alpha2, alpha3, name):
        self.alpha2 = alpha2
        self.alpha3 = alpha3
        self.name = name
        super(Country, self).__init__(alpha2, alpha3, name)

    def __str__(self):
        return self.alpha2

    def __repr__(self):
        return u"<Country '{0.name}' ({0.alpha2}) ({0.alpha3})>".format(self)


#    ISO-639-1, ISO-639-2/B, ISO-639-2/T, English Name
Language("ab", "abk", "abk", u"Abkhazian")
Language("aa", "aar", "aar", u"Afar")
Language("af", "afr", "afr", u"Afrikaans")
Language("ak", "aka", "aka", u"Akan")
Language("sq", "alb", "sqi", u"Albanian")
Language("am", "amh", "amh", u"Amharic")
Language("ar", "ara", "ara", u"Arabic")
Language("an", "arg", "arg", u"Aragonese")
Language("hy", "arm", "hye", u"Armenian")
Language("as", "asm", "asm", u"Assamese")
Language("av", "ava", "ava", u"Avaric")
Language("ae", "ave", "ave", u"Avestan")
Language("ay", "aym", "aym", u"Aymara")
Language("az", "aze", "aze", u"Azerbaijani")
Language("bm", "bam", "bam", u"Bambara")
Language("ba", "bak", "bak", u"Bashkir")
Language("eu", "baq", "eus", u"Basque")
Language("be", "bel", "bel", u"Belarusian")
Language("bn", "ben", "ben", u"Bengali")
Language("bh", "bih", "bih", u"Bihari languages")
Language("bi", "bis", "bis", u"Bislama")
Language("nb", "nob", "nob", u"Bokmål, Norwegian")
Language("bs", "bos", "bos", u"Bosnian")
Language("br", "bre", "bre", u"Breton")
Language("bg", "bul", "bul", u"Bulgarian")
Language("my", "bur", "mya", u"Burmese")
Language("es", "spa", "spa", u"Castilian")
Language("ca", "cat", "cat", u"Catalan")
Language("km", "khm", "khm", u"Central Khmer")
Language("ch", "cha", "cha", u"Chamorro")
Language("ce", "che", "che", u"Chechen")
Language("ny", "nya", "nya", u"Chewa")
Language("ny", "nya", "nya", u"Chichewa")
Language("zh", "chi", "zho", u"Chinese")
Language("za", "zha", "zha", u"Chuang")
Language("cu", "chu", "chu", u"Church Slavic")
Language("cu", "chu", "chu", u"Church Slavonic")
Language("cv", "chv", "chv", u"Chuvash")
Language("kw", "cor", "cor", u"Cornish")
Language("co", "cos", "cos", u"Corsican")
Language("cr", "cre", "cre", u"Cree")
Language("hr", "hrv", "hrv", u"Croatian")
Language("cs", "cze", "ces", u"Czech")
Language("da", "dan", "dan", u"Danish")
Language("dv", "div", "div", u"Dhivehi")
Language("dv", "div", "div", u"Divehi")
Language("nl", "dut", "nld", u"Dutch")
Language("dz", "dzo", "dzo", u"Dzongkha")
Language("en", "eng", "eng", u"English")
Language("eo", "epo", "epo", u"Esperanto")
Language("et", "est", "est", u"Estonian")
Language("ee", "ewe", "ewe", u"Ewe")
Language("fo", "fao", "fao", u"Faroese")
Language("fj", "fij", "fij", u"Fijian")
Language("fi", "fin", "fin", u"Finnish")
Language("nl", "dut", "nld", u"Flemish")
Language("fr", "fre", "fra", u"French")
Language("ff", "ful", "ful", u"Fulah")
Language("gd", "gla", "gla", u"Gaelic")
Language("gl", "glg", "glg", u"Galician")
Language("lg", "lug", "lug", u"Ganda")
Language("ka", "geo", "kat", u"Georgian")
Language("de", "ger", "deu", u"German")
Language("ki", "kik", "kik", u"Gikuyu")
Language("el", "gre", "ell", u"Greek, Modern (1453-)")
Language("kl", "kal", "kal", u"Greenlandic")
Language("gn", "grn", "grn", u"Guarani")
Language("gu", "guj", "guj", u"Gujarati")
Language("ht", "hat", "hat", u"Haitian")
Language("ht", "hat", "hat", u"Haitian Creole")
Language("ha", "hau", "hau", u"Hausa")
Language("he", "heb", "heb", u"Hebrew")
Language("hz", "her", "her", u"Herero")
Language("hi", "hin", "hin", u"Hindi")
Language("ho", "hmo", "hmo", u"Hiri Motu")
Language("hu", "hun", "hun", u"Hungarian")
Language("is", "ice", "isl", u"Icelandic")
Language("io", "ido", "ido", u"Ido")
Language("ig", "ibo", "ibo", u"Igbo")
Language("id", "ind", "ind", u"Indonesian")
Language("ia", "ina", "ina", u"Interlingua (International Auxiliary Language Association)")
Language("ie", "ile", "ile", u"Interlingue")
Language("iu", "iku", "iku", u"Inuktitut")
Language("ik", "ipk", "ipk", u"Inupiaq")
Language("ga", "gle", "gle", u"Irish")
Language("it", "ita", "ita", u"Italian")
Language("ja", "jpn", "jpn", u"Japanese")
Language("jv", "jav", "jav", u"Javanese")
Language("kl", "kal", "kal", u"Kalaallisut")
Language("kn", "kan", "kan", u"Kannada")
Language("kr", "kau", "kau", u"Kanuri")
Language("ks", "kas", "kas", u"Kashmiri")
Language("kk", "kaz", "kaz", u"Kazakh")
Language("ki", "kik", "kik", u"Kikuyu")
Language("rw", "kin", "kin", u"Kinyarwanda")
Language("ky", "kir", "kir", u"Kirghiz")
Language("kv", "kom", "kom", u"Komi")
Language("kg", "kon", "kon", u"Kongo")
Language("ko", "kor", "kor", u"Korean")
Language("kj", "kua", "kua", u"Kuanyama")
Language("ku", "kur", "kur", u"Kurdish")
Language("kj", "kua", "kua", u"Kwanyama")
Language("ky", "kir", "kir", u"Kyrgyz")
Language("lo", "lao", "lao", u"Lao")
Language("la", "lat", "lat", u"Latin")
Language("lv", "lav", "lav", u"Latvian")
Language("lb", "ltz", "ltz", u"Letzeburgesch")
Language("li", "lim", "lim", u"Limburgan")
Language("li", "lim", "lim", u"Limburger")
Language("li", "lim", "lim", u"Limburgish")
Language("ln", "lin", "lin", u"Lingala")
Language("lt", "lit", "lit", u"Lithuanian")
Language("lu", "lub", "lub", u"Luba-Katanga")
Language("lb", "ltz", "ltz", u"Luxembourgish")
Language("mk", "mac", "mkd", u"Macedonian")
Language("mg", "mlg", "mlg", u"Malagasy")
Language("ms", "may", "msa", u"Malay")
Language("ml", "mal", "mal", u"Malayalam")
Language("dv", "div", "div", u"Maldivian")
Language("mt", "mlt", "mlt", u"Maltese")
Language("gv", "glv", "glv", u"Manx")
Language("mi", "mao", "mri", u"Maori")
Language("mr", "mar", "mar", u"Marathi")
Language("mh", "mah", "mah", u"Marshallese")
Language("ro", "rum", "ron", u"Moldavian")
Language("ro", "rum", "ron", u"Moldovan")
Language("mn", "mon", "mon", u"Mongolian")
Language("na", "nau", "nau", u"Nauru")
Language("nv", "nav", "nav", u"Navaho")
Language("nv", "nav", "nav", u"Navajo")
Language("nd", "nde", "nde", u"Ndebele, North")
Language("nr", "nbl", "nbl", u"Ndebele, South")
Language("ng", "ndo", "ndo", u"Ndonga")
Language("ne", "nep", "nep", u"Nepali")
Language("nd", "nde", "nde", u"North Ndebele")
Language("se", "sme", "sme", u"Northern Sami")
Language("no", "nor", "nor", u"Norwegian")
Language("nb", "nob", "nob", u"Norwegian Bokmål")
Language("nn", "nno", "nno", u"Norwegian Nynorsk")
Language("ii", "iii", "iii", u"Nuosu")
Language("ny", "nya", "nya", u"Nyanja")
Language("nn", "nno", "nno", u"Nynorsk, Norwegian")
Language("ie", "ile", "ile", u"Occidental")
Language("oc", "oci", "oci", u"Occitan (post 1500)")
Language("oj", "oji", "oji", u"Ojibwa")
Language("cu", "chu", "chu", u"Old Bulgarian")
Language("cu", "chu", "chu", u"Old Church Slavonic")
Language("cu", "chu", "chu", u"Old Slavonic")
Language("or", "ori", "ori", u"Oriya")
Language("om", "orm", "orm", u"Oromo")
Language("os", "oss", "oss", u"Ossetian")
Language("os", "oss", "oss", u"Ossetic")
Language("pi", "pli", "pli", u"Pali")
Language("pa", "pan", "pan", u"Panjabi")
Language("ps", "pus", "pus", u"Pashto")
Language("fa", "per", "fas", u"Persian")
Language("pl", "pol", "pol", u"Polish")
Language("pt", "por", "por", u"Portuguese")
Language("pa", "pan", "pan", u"Punjabi")
Language("ps", "pus", "pus", u"Pushto")
Language("qu", "que", "que", u"Quechua")
Language("ro", "rum", "ron", u"Romanian")
Language("rm", "roh", "roh", u"Romansh")
Language("rn", "run", "run", u"Rundi")
Language("ru", "rus", "rus", u"Russian")
Language("sm", "smo", "smo", u"Samoan")
Language("sg", "sag", "sag", u"Sango")
Language("sa", "san", "san", u"Sanskrit")
Language("sc", "srd", "srd", u"Sardinian")
Language("gd", "gla", "gla", u"Scottish Gaelic")
Language("sr", "srp", "srp", u"Serbian")
Language("sn", "sna", "sna", u"Shona")
Language("ii", "iii", "iii", u"Sichuan Yi")
Language("sd", "snd", "snd", u"Sindhi")
Language("si", "sin", "sin", u"Sinhala")
Language("si", "sin", "sin", u"Sinhalese")
Language("sk", "slo", "slk", u"Slovak")
Language("sl", "slv", "slv", u"Slovenian")
Language("so", "som", "som", u"Somali")
Language("st", "sot", "sot", u"Sotho, Southern")
Language("nr", "nbl", "nbl", u"South Ndebele")
Language("es", "spa", "spa", u"Spanish")
Language("su", "sun", "sun", u"Sundanese")
Language("sw", "swa", "swa", u"Swahili")
Language("ss", "ssw", "ssw", u"Swati")
Language("sv", "swe", "swe", u"Swedish")
Language("tl", "tgl", "tgl", u"Tagalog")
Language("ty", "tah", "tah", u"Tahitian")
Language("tg", "tgk", "tgk", u"Tajik")
Language("ta", "tam", "tam", u"Tamil")
Language("tt", "tat", "tat", u"Tatar")
Language("te", "tel", "tel", u"Telugu")
Language("th", "tha", "tha", u"Thai")
Language("bo", "tib", "bod", u"Tibetan")
Language("ti", "tir", "tir", u"Tigrinya")
Language("to", "ton", "ton", u"Tonga (Tonga Islands)")
Language("ts", "tso", "tso", u"Tsonga")
Language("tn", "tsn", "tsn", u"Tswana")
Language("tr", "tur", "tur", u"Turkish")
Language("tk", "tuk", "tuk", u"Turkmen")
Language("tw", "twi", "twi", u"Twi")
Language("ug", "uig", "uig", u"Uighur")
Language("uk", "ukr", "ukr", u"Ukrainian")
Language("ur", "urd", "urd", u"Urdu")
Language("ug", "uig", "uig", u"Uyghur")
Language("uz", "uzb", "uzb", u"Uzbek")
Language("ca", "cat", "cat", u"Valencian")
Language("ve", "ven", "ven", u"Venda")
Language("vi", "vie", "vie", u"Vietnamese")
Language("vo", "vol", "vol", u"Volapük")
Language("wa", "wln", "wln", u"Walloon")
Language("cy", "wel", "cym", u"Welsh")
Language("fy", "fry", "fry", u"Western Frisian")
Language("wo", "wol", "wol", u"Wolof")
Language("xh", "xho", "xho", u"Xhosa")
Language("yi", "yid", "yid", u"Yiddish")
Language("yo", "yor", "yor", u"Yoruba")
Language("za", "zha", "zha", u"Zhuang")
Language("zu", "zul", "zul", u"Zulu")



# ISO-3166 alpha-2, alpha-3, English Name
Country("AF", "AFG", u"Afghanistan")
Country("AL", "ALB", u"Albania")
Country("DZ", "DZA", u"Algeria")
Country("AS", "ASM", u"American Samoa")
Country("AD", "AND", u"Andorra")
Country("AO", "AGO", u"Angola")
Country("AI", "AIA", u"Anguilla")
Country("AQ", "ATA", u"Antarctica")
Country("AG", "ATG", u"Antigua and Barbuda")
Country("AR", "ARG", u"Argentina")
Country("AM", "ARM", u"Armenia")
Country("AW", "ABW", u"Aruba")
Country("AU", "AUS", u"Australia")
Country("AT", "AUT", u"Austria")
Country("AZ", "AZE", u"Azerbaijan")
Country("BS", "BHS", u"Bahamas (the)")
Country("BH", "BHR", u"Bahrain")
Country("BD", "BGD", u"Bangladesh")
Country("BB", "BRB", u"Barbados")
Country("BY", "BLR", u"Belarus")
Country("BE", "BEL", u"Belgium")
Country("BZ", "BLZ", u"Belize")
Country("BJ", "BEN", u"Benin")
Country("BM", "BMU", u"Bermuda")
Country("BT", "BTN", u"Bhutan")
Country("BO", "BOL", u"Bolivia (Plurinational State of)")
Country("BQ", "BES", u"Bonaire, Sint Eustatius and Saba")
Country("BA", "BIH", u"Bosnia and Herzegovina")
Country("BW", "BWA", u"Botswana")
Country("BV", "BVT", u"Bouvet Island")
Country("BR", "BRA", u"Brazil")
Country("IO", "IOT", u"British Indian Ocean Territory (the)")
Country("BN", "BRN", u"Brunei Darussalam")
Country("BG", "BGR", u"Bulgaria")
Country("BF", "BFA", u"Burkina Faso")
Country("BI", "BDI", u"Burundi")
Country("CV", "CPV", u"Cabo Verde")
Country("KH", "KHM", u"Cambodia")
Country("CM", "CMR", u"Cameroon")
Country("CA", "CAN", u"Canada")
Country("KY", "CYM", u"Cayman Islands (the)")
Country("CF", "CAF", u"Central African Republic (the)")
Country("TD", "TCD", u"Chad")
Country("CL", "CHL", u"Chile")
Country("CN", "CHN", u"China")
Country("CX", "CXR", u"Christmas Island")
Country("CC", "CCK", u"Cocos (Keeling) Islands (the)")
Country("CO", "COL", u"Colombia")
Country("KM", "COM", u"Comoros (the)")
Country("CD", "COD", u"Congo (the Democratic Republic of the)")
Country("CG", "COG", u"Congo (the)")
Country("CK", "COK", u"Cook Islands (the)")
Country("CR", "CRI", u"Costa Rica")
Country("HR", "HRV", u"Croatia")
Country("CU", "CUB", u"Cuba")
Country("CW", "CUW", u"Curaçao")
Country("CY", "CYP", u"Cyprus")
Country("CZ", "CZE", u"Czechia")
Country("CI", "CIV", u"Côte d'Ivoire")
Country("DK", "DNK", u"Denmark")
Country("DJ", "DJI", u"Djibouti")
Country("DM", "DMA", u"Dominica")
Country("DO", "DOM", u"Dominican Republic (the)")
Country("EC", "ECU", u"Ecuador")
Country("EG", "EGY", u"Egypt")
Country("SV", "SLV", u"El Salvador")
Country("GQ", "GNQ", u"Equatorial Guinea")
Country("ER", "ERI", u"Eritrea")
Country("EE", "EST", u"Estonia")
Country("SZ", "SWZ", u"Eswatini")
Country("ET", "ETH", u"Ethiopia")
Country("FK", "FLK", u"Falkland Islands (the) [Malvinas]")
Country("FO", "FRO", u"Faroe Islands (the)")
Country("FJ", "FJI", u"Fiji")
Country("FI", "FIN", u"Finland")
Country("FR", "FRA", u"France")
Country("GF", "GUF", u"French Guiana")
Country("PF", "PYF", u"French Polynesia")
Country("TF", "ATF", u"French Southern Territories (the)")
Country("GA", "GAB", u"Gabon")
Country("GM", "GMB", u"Gambia (the)")
Country("GE", "GEO", u"Georgia")
Country("DE", "DEU", u"Germany")
Country("GH", "GHA", u"Ghana")
Country("GI", "GIB", u"Gibraltar")
Country("GR", "GRC", u"Greece")
Country("GL", "GRL", u"Greenland")
Country("GD", "GRD", u"Grenada")
Country("GP", "GLP", u"Guadeloupe")
Country("GU", "GUM", u"Guam")
Country("GT", "GTM", u"Guatemala")
Country("GG", "GGY", u"Guernsey")
Country("GN", "GIN", u"Guinea")
Country("GW", "GNB", u"Guinea-Bissau")
Country("GY", "GUY", u"Guyana")
Country("HT", "HTI", u"Haiti")
Country("HM", "HMD", u"Heard Island and McDonald Islands")
Country("VA", "VAT", u"Holy See (the)")
Country("HN", "HND", u"Honduras")
Country("HK", "HKG", u"Hong Kong")
Country("HU", "HUN", u"Hungary")
Country("IS", "ISL", u"Iceland")
Country("IN", "IND", u"India")
Country("ID", "IDN", u"Indonesia")
Country("IR", "IRN", u"Iran (Islamic Republic of)")
Country("IQ", "IRQ", u"Iraq")
Country("IE", "IRL", u"Ireland")
Country("IM", "IMN", u"Isle of Man")
Country("IL", "ISR", u"Israel")
Country("IT", "ITA", u"Italy")
Country("JM", "JAM", u"Jamaica")
Country("JP", "JPN", u"Japan")
Country("JE", "JEY", u"Jersey")
Country("JO", "JOR", u"Jordan")
Country("KZ", "KAZ", u"Kazakhstan")
Country("KE", "KEN", u"Kenya")
Country("KI", "KIR", u"Kiribati")
Country("KP", "PRK", u"Korea (the Democratic People's Republic of)")
Country("KR", "KOR", u"Korea (the Republic of)")
Country("KW", "KWT", u"Kuwait")
Country("KG", "KGZ", u"Kyrgyzstan")
Country("LA", "LAO", u"Lao People's Democratic Republic (the)")
Country("LV", "LVA", u"Latvia")
Country("LB", "LBN", u"Lebanon")
Country("LS", "LSO", u"Lesotho")
Country("LR", "LBR", u"Liberia")
Country("LY", "LBY", u"Libya")
Country("LI", "LIE", u"Liechtenstein")
Country("LT", "LTU", u"Lithuania")
Country("LU", "LUX", u"Luxembourg")
Country("MO", "MAC", u"Macao")
Country("MG", "MDG", u"Madagascar")
Country("MW", "MWI", u"Malawi")
Country("MY", "MYS", u"Malaysia")
Country("MV", "MDV", u"Maldives")
Country("ML", "MLI", u"Mali")
Country("MT", "MLT", u"Malta")
Country("MH", "MHL", u"Marshall Islands (the)")
Country("MQ", "MTQ", u"Martinique")
Country("MR", "MRT", u"Mauritania")
Country("MU", "MUS", u"Mauritius")
Country("YT", "MYT", u"Mayotte")
Country("MX", "MEX", u"Mexico")
Country("FM", "FSM", u"Micronesia (Federated States of)")
Country("MD", "MDA", u"Moldova (the Republic of)")
Country("MC", "MCO", u"Monaco")
Country("MN", "MNG", u"Mongolia")
Country("ME", "MNE", u"Montenegro")
Country("MS", "MSR", u"Montserrat")
Country("MA", "MAR", u"Morocco")
Country("MZ", "MOZ", u"Mozambique")
Country("MM", "MMR", u"Myanmar")
Country("NA", "NAM", u"Namibia")
Country("NR", "NRU", u"Nauru")
Country("NP", "NPL", u"Nepal")
Country("NL", "NLD", u"Netherlands (the)")
Country("NC", "NCL", u"New Caledonia")
Country("NZ", "NZL", u"New Zealand")
Country("NI", "NIC", u"Nicaragua")
Country("NE", "NER", u"Niger (the)")
Country("NG", "NGA", u"Nigeria")
Country("NU", "NIU", u"Niue")
Country("NF", "NFK", u"Norfolk Island")
Country("MP", "MNP", u"Northern Mariana Islands (the)")
Country("NO", "NOR", u"Norway")
Country("OM", "OMN", u"Oman")
Country("PK", "PAK", u"Pakistan")
Country("PW", "PLW", u"Palau")
Country("PS", "PSE", u"Palestine, State of")
Country("PA", "PAN", u"Panama")
Country("PG", "PNG", u"Papua New Guinea")
Country("PY", "PRY", u"Paraguay")
Country("PE", "PER", u"Peru")
Country("PH", "PHL", u"Philippines (the)")
Country("PN", "PCN", u"Pitcairn")
Country("PL", "POL", u"Poland")
Country("PT", "PRT", u"Portugal")
Country("PR", "PRI", u"Puerto Rico")
Country("QA", "QAT", u"Qatar")
Country("MK", "MKD", u"Republic of North Macedonia")
Country("RO", "ROU", u"Romania")
Country("RU", "RUS", u"Russian Federation (the)")
Country("RW", "RWA", u"Rwanda")
Country("RE", "REU", u"Réunion")
Country("BL", "BLM", u"Saint Barthélemy")
Country("SH", "SHN", u"Saint Helena, Ascension and Tristan da Cunha")
Country("KN", "KNA", u"Saint Kitts and Nevis")
Country("LC", "LCA", u"Saint Lucia")
Country("MF", "MAF", u"Saint Martin (French part)")
Country("PM", "SPM", u"Saint Pierre and Miquelon")
Country("VC", "VCT", u"Saint Vincent and the Grenadines")
Country("WS", "WSM", u"Samoa")
Country("SM", "SMR", u"San Marino")
Country("ST", "STP", u"Sao Tome and Principe")
Country("SA", "SAU", u"Saudi Arabia")
Country("SN", "SEN", u"Senegal")
Country("RS", "SRB", u"Serbia")
Country("SC", "SYC", u"Seychelles")
Country("SL", "SLE", u"Sierra Leone")
Country("SG", "SGP", u"Singapore")
Country("SX", "SXM", u"Sint Maarten (Dutch part)")
Country("SK", "SVK", u"Slovakia")
Country("SI", "SVN", u"Slovenia")
Country("SB", "SLB", u"Solomon Islands")
Country("SO", "SOM", u"Somalia")
Country("ZA", "ZAF", u"South Africa")
Country("GS", "SGS", u"South Georgia and the South Sandwich Islands")
Country("SS", "SSD", u"South Sudan")
Country("ES", "ESP", u"Spain")
Country("LK", "LKA", u"Sri Lanka")
Country("SD", "SDN", u"Sudan (the)")
Country("SR", "SUR", u"Suriname")
Country("SJ", "SJM", u"Svalbard and Jan Mayen")
Country("SE", "SWE", u"Sweden")
Country("CH", "CHE", u"Switzerland")
Country("SY", "SYR", u"Syrian Arab Republic")
Country("TW", "TWN", u"Taiwan (Province of China)")
Country("TJ", "TJK", u"Tajikistan")
Country("TZ", "TZA", u"Tanzania, United Republic of")
Country("TH", "THA", u"Thailand")
Country("TL", "TLS", u"Timor-Leste")
Country("TG", "TGO", u"Togo")
Country("TK", "TKL", u"Tokelau")
Country("TO", "TON", u"Tonga")
Country("TT", "TTO", u"Trinidad and Tobago")
Country("TN", "TUN", u"Tunisia")
Country("TR", "TUR", u"Turkey")
Country("TM", "TKM", u"Turkmenistan")
Country("TC", "TCA", u"Turks and Caicos Islands (the)")
Country("TV", "TUV", u"Tuvalu")
Country("UG", "UGA", u"Uganda")
Country("UA", "UKR", u"Ukraine")
Country("AE", "ARE", u"United Arab Emirates (the)")
Country("GB", "GBR", u"United Kingdom of Great Britain and Northern Ireland (the)")
Country("UM", "UMI", u"United States Minor Outlying Islands (the)")
Country("US", "USA", u"United States of America (the)")
Country("UY", "URY", u"Uruguay")
Country("UZ", "UZB", u"Uzbekistan")
Country("VU", "VUT", u"Vanuatu")
Country("VE", "VEN", u"Venezuela (Bolivarian Republic of)")
Country("VN", "VNM", u"Viet Nam")
Country("VG", "VGB", u"Virgin Islands (British)")
Country("VI", "VIR", u"Virgin Islands (U.S.)")
Country("WF", "WLF", u"Wallis and Futuna")
Country("EH", "ESH", u"Western Sahara")
Country("YE", "YEM", u"Yemen")
Country("ZM", "ZMB", u"Zambia")
Country("ZW", "ZWE", u"Zimbabwe")
Country("AX", "ALA", u"Åland Islands")



if __name__ == '__main__':

    print(repr(Language.getstored('de')))
    print(repr(Language.getstored('hrv')))
    print(repr(Language.getstored('Danish')))
    print(repr(Language.getstored('sqi')))
    print(repr(Language.getstored("alb")))
    print(repr(Language.getstored("German")))

    print(Language.getstored('hrv').ISO639_1)
    print(Language.getstored('de').ISO639_2B)
    print(Language.getstored('de').ISO639_2T)
    print(Language.getstored('deu').ISO639_2T)

    print(repr(Country.getstored('ax')))
    print(Country.getstored('AX').name)
    print(Country.getstored('ALA').alpha2)
    print(Country.getstored(u"Åland Islands").alpha3)

    print(repr(Country.getstored('MC')))
    print(Country.getstored('MC').name)
    print(Country.getstored('mco').alpha2)
    print(Country.getstored(u"Monaco").alpha3)
