
#include "iso3166.h"

static CodeToNameMap createCountryMap(void)
{
    // TODO: List is incomplete!
    // Translations manually extracted from Debian iso-codes repo.

    // The names were chosen according to the official language of each
    // country, therefore countries with multiple _official_ languages have
    // been omitted for now.

    // A number of other countries are simply missing e.g. Most of
    // central/southern Africa, Western Asia, S.E. Asia and various countries
    // in other regions
    CodeToNameMap map;
    map["AE"] = QString::fromUtf8("الإمارات العربيّة المتحدّة");   // United Arab Emirates
    map["AR"] = QString::fromUtf8("Argentina");   // Argentina
    map["AT"] = QString::fromUtf8("Österreich");   // Austria
    map["AU"] = QString::fromUtf8("Australia");   // Australia
    map["BG"] = QString::fromUtf8("България");   // Bulgaria
    map["BH"] = QString::fromUtf8("البحرين");   // Bahrain
    map["BR"] = QString::fromUtf8("Brasil");   // Brazil
    map["BY"] = QString::fromUtf8("Беларусь");   // Belarus
    map["CA"] = QString::fromUtf8("Canada");   // Canada
    map["CL"] = QString::fromUtf8("Chile");   // Chile
    map["CN"] = QString::fromUtf8("中國");   // China
    map["CO"] = QString::fromUtf8("Colombia");   // Colombia
    map["CZ"] = QString::fromUtf8("Česká republika");   // Czech Republic
    map["DE"] = QString::fromUtf8("Deutschland");   // Germany
    map["DK"] = QString::fromUtf8("Danmark");   // Denmark
    map["DZ"] = QString::fromUtf8("الجزائر");   // Algeria
    map["EG"] = QString::fromUtf8("مصر");   // Egypt
    map["EH"] = QString::fromUtf8("الصّحراء الغربيّة");   // Western Sahara
    map["ES"] = QString::fromUtf8("España");   // Spain
    map["ET"] = QString::fromUtf8("Eesti");   // Estonia
    map["FI"] = QString::fromUtf8("Suomi");   // Finland
    map["FR"] = QString::fromUtf8("France");   // France
    map["GB"] = QString::fromUtf8("United Kingdom");   // United Kingdom
    map["GR"] = QString::fromUtf8("Ελλάδα");   // Greece
    map["HK"] = QString::fromUtf8("Hong Kong, 香港");   // Hong Kong
    map["HR"] = QString::fromUtf8("Hrvatska");   // Croatia
    map["HU"] = QString::fromUtf8("Magyarország");   // Hungary
    map["IL"] = QString::fromUtf8("ישראל");   // Israel
    map["IN"] = QString::fromUtf8("भारत");   // India (Hindi)
    map["IS"] = QString::fromUtf8("Ísland");   // Iceland
    map["IT"] = QString::fromUtf8("Italia");   // Italy
    map["JM"] = QString::fromUtf8("Jamaica");   // Jamaica
    map["JO"] = QString::fromUtf8("الأردن");   // Jordan
    map["JP"] = QString::fromUtf8("日本");   // Japan
    map["KP"] = QString::fromUtf8("조선민주주의인민공화국");   // Korea, Democratic People's Republic of
    map["KR"] = QString::fromUtf8("대한민국");   // Korea, Republic of
    map["KW"] = QString::fromUtf8("الكويت");   // Kuwait
    map["LB"] = QString::fromUtf8("لبنان");   // Lebanon
    map["IR"] = QString::fromUtf8("جمهوری اسلامی ایران");   // Iran, Islamic Republic of
    map["LT"] = QString::fromUtf8("Lietuva");   // Lithuania
    map["LV"] = QString::fromUtf8("Latvija");   // Latvia
    map["LY"] = QString::fromUtf8("الجماهيريّة العربيّة اللّيبيّة");   // Libyan Arab Jamahiriya
    map["MA"] = QString::fromUtf8("المغرب");   // Morocco
    map["MC"] = QString::fromUtf8("Monaco");   // Monaco
    map["MR"] = QString::fromUtf8("موريتانيا");   // Mauritania
    map["MX"] = QString::fromUtf8("México");   // Mexico
    map["NL"] = QString::fromUtf8("Nederland");   // Netherlands
    map["NO"] = QString::fromUtf8("Norge");   // Norway
    map["NZ"] = QString::fromUtf8("New Zealand");   // New Zealand
    map["OM"] = QString::fromUtf8("عمان");   // Oman
    map["PL"] = QString::fromUtf8("Polska");   // Poland
    map["PR"] = QString::fromUtf8("Puerto Rico");   // Puerto Rico
    map["PT"] = QString::fromUtf8("Portugal");   // Portugal
    map["PY"] = QString::fromUtf8("Paraguay");   // Paraguay
    map["QA"] = QString::fromUtf8("قطر");   // Qatar
    map["RU"] = QString::fromUtf8("Российская Федерация");   // Russian Federation
    map["SA"] = QString::fromUtf8("السّعوديّة");   // Saudi Arabia
    map["SE"] = QString::fromUtf8("Sverige");   // Sweden
    map["SI"] = QString::fromUtf8("Slovenija");   // Slovenia
    map["SK"] = QString::fromUtf8("Slovensko");   // Slovakia
    map["SY"] = QString::fromUtf8("الجمهوريّة العربيّة السّوريّة");   // Syrian Arab Republic
    map["TH"] = QString::fromUtf8("ไทย");   // Thailand
    map["TN"] = QString::fromUtf8("تونس");   // Tunisia
    map["TR"] = QString::fromUtf8("Türkiye");   // Turkey
    map["TW"] = QString::fromUtf8("台灣");   // Taiwan
    map["UA"] = QString::fromUtf8("Україна");   // Ukraine
    map["US"] = QString::fromUtf8("United States");   // United States
    map["UY"] = QString::fromUtf8("Uruguay");   // Uruguay
    map["VN"] = QString::fromUtf8("Việt Nam");   // Vietnam
    map["YE"] = QString::fromUtf8("اليمن");   // Yemen
    return map;
}

static CodeToNameMap gCountryMap;

static CodeToNameMap createEnglishCountryMap(void)
{
    CodeToNameMap map;
    map["AD"] = QString::fromUtf8("Andorra");
    map["AE"] = QString::fromUtf8("United Arab Emirates");
    map["AF"] = QString::fromUtf8("Afghanistan");
    map["AG"] = QString::fromUtf8("Antigua and Barbuda");
    map["AI"] = QString::fromUtf8("Anguilla");
    map["AL"] = QString::fromUtf8("Albania");
    map["AM"] = QString::fromUtf8("Armenia");
    map["AN"] = QString::fromUtf8("Netherlands Antilles");
    map["AO"] = QString::fromUtf8("Angola");
    map["AQ"] = QString::fromUtf8("Antarctica");
    map["AR"] = QString::fromUtf8("Argentina");
    map["AS"] = QString::fromUtf8("American Samoa");
    map["AT"] = QString::fromUtf8("Austria");
    map["AU"] = QString::fromUtf8("Australia");
    map["AW"] = QString::fromUtf8("Aruba");
    map["AX"] = QString::fromUtf8("Aland Islands");
    map["AZ"] = QString::fromUtf8("Azerbaijan");
    map["BA"] = QString::fromUtf8("Bosnia and Herzegovina");
    map["BB"] = QString::fromUtf8("Barbados");
    map["BD"] = QString::fromUtf8("Bangladesh");
    map["BE"] = QString::fromUtf8("Belgium");
    map["BF"] = QString::fromUtf8("Burkina Faso");
    map["BG"] = QString::fromUtf8("Bulgaria");
    map["BH"] = QString::fromUtf8("Bahrain");
    map["BI"] = QString::fromUtf8("Burundi");
    map["BJ"] = QString::fromUtf8("Benin");
    map["BM"] = QString::fromUtf8("Bermuda");
    map["BN"] = QString::fromUtf8("Brunei Darussalam");
    map["BO"] = QString::fromUtf8("Bolivia");
    map["BR"] = QString::fromUtf8("Brazil");
    map["BS"] = QString::fromUtf8("Bahamas");
    map["BT"] = QString::fromUtf8("Bhutan");
    map["BV"] = QString::fromUtf8("Bouvet Island");
    map["BW"] = QString::fromUtf8("Botswana");
    map["BY"] = QString::fromUtf8("Belarus");
    map["BZ"] = QString::fromUtf8("Belize");
    map["CA"] = QString::fromUtf8("Canada");
    map["CC"] = QString::fromUtf8("Cocos (Keeling) Islands");
    map["CD"] = QString::fromUtf8("Congo, The Democratic Republic of the");
    map["CF"] = QString::fromUtf8("Central African Republic");
    map["CG"] = QString::fromUtf8("Congo");
    map["CH"] = QString::fromUtf8("Switzerland");
    map["CI"] = QString::fromUtf8("Cote D'Ivoire");
    map["CK"] = QString::fromUtf8("Cook Islands");
    map["CL"] = QString::fromUtf8("Chile");
    map["CM"] = QString::fromUtf8("Cameroon");
    map["CN"] = QString::fromUtf8("China");
    map["CO"] = QString::fromUtf8("Colombia");
    map["CR"] = QString::fromUtf8("Costa Rica");
    map["CS"] = QString::fromUtf8("Serbia and Montenegro");
    map["CU"] = QString::fromUtf8("Cuba");
    map["CV"] = QString::fromUtf8("Cape Verde");
    map["CX"] = QString::fromUtf8("Christmas Island");
    map["CY"] = QString::fromUtf8("Cyprus");
    map["CZ"] = QString::fromUtf8("Czech Republic");
    map["DE"] = QString::fromUtf8("Germany");
    map["DJ"] = QString::fromUtf8("Djibouti");
    map["DK"] = QString::fromUtf8("Denmark");
    map["DM"] = QString::fromUtf8("Dominica");
    map["DO"] = QString::fromUtf8("Dominican Republic");
    map["DZ"] = QString::fromUtf8("Algeria");
    map["EC"] = QString::fromUtf8("Ecuador");
    map["EE"] = QString::fromUtf8("Estonia");
    map["EG"] = QString::fromUtf8("Egypt");
    map["EH"] = QString::fromUtf8("Western Sahara");
    map["ER"] = QString::fromUtf8("Eritrea");
    map["ES"] = QString::fromUtf8("Spain");
    map["ET"] = QString::fromUtf8("Ethiopia");
    map["FI"] = QString::fromUtf8("Finland");
    map["FJ"] = QString::fromUtf8("Fiji");
    map["FK"] = QString::fromUtf8("Falkland Islands (Malvinas)");
    map["FM"] = QString::fromUtf8("Micronesia, Federated States of");
    map["FO"] = QString::fromUtf8("Faroe Islands");
    map["FR"] = QString::fromUtf8("France");
    map["FX"] = QString::fromUtf8("France, Metropolitan");
    map["GA"] = QString::fromUtf8("Gabon");
    map["GB"] = QString::fromUtf8("United Kingdom");
    map["GD"] = QString::fromUtf8("Grenada");
    map["GE"] = QString::fromUtf8("Georgia");
    map["GF"] = QString::fromUtf8("French Guiana");
    map["GH"] = QString::fromUtf8("Ghana");
    map["GI"] = QString::fromUtf8("Gibraltar");
    map["GL"] = QString::fromUtf8("Greenland");
    map["GM"] = QString::fromUtf8("Gambia");
    map["GN"] = QString::fromUtf8("Guinea");
    map["GP"] = QString::fromUtf8("Guadeloupe");
    map["GQ"] = QString::fromUtf8("Equatorial Guinea");
    map["GR"] = QString::fromUtf8("Greece");
    map["GS"] = QString::fromUtf8("South Georgia and the South Sandwich Islands");
    map["GT"] = QString::fromUtf8("Guatemala");
    map["GU"] = QString::fromUtf8("Guam");
    map["GW"] = QString::fromUtf8("Guinea-Bissau");
    map["GY"] = QString::fromUtf8("Guyana");
    map["HK"] = QString::fromUtf8("Hong Kong");
    map["HM"] = QString::fromUtf8("Heard Island and McDonald Islands");
    map["HN"] = QString::fromUtf8("Honduras");
    map["HR"] = QString::fromUtf8("Croatia");
    map["HT"] = QString::fromUtf8("Haiti");
    map["HU"] = QString::fromUtf8("Hungary");
    map["ID"] = QString::fromUtf8("Indonesia");
    map["IE"] = QString::fromUtf8("Ireland");
    map["IL"] = QString::fromUtf8("Israel");
    map["IN"] = QString::fromUtf8("India");
    map["IO"] = QString::fromUtf8("British Indian Ocean Territory");
    map["IQ"] = QString::fromUtf8("Iraq");
    map["IR"] = QString::fromUtf8("Iran, Islamic Republic of");
    map["IS"] = QString::fromUtf8("Iceland");
    map["IT"] = QString::fromUtf8("Italy");
    map["JM"] = QString::fromUtf8("Jamaica");
    map["JO"] = QString::fromUtf8("Jordan");
    map["JP"] = QString::fromUtf8("Japan");
    map["KE"] = QString::fromUtf8("Kenya");
    map["KG"] = QString::fromUtf8("Kyrgyzstan");
    map["KH"] = QString::fromUtf8("Cambodia");
    map["KI"] = QString::fromUtf8("Kiribati");
    map["KM"] = QString::fromUtf8("Comoros");
    map["KN"] = QString::fromUtf8("Saint Kitts and Nevis");
    map["KP"] = QString::fromUtf8("Korea, Democratic People's Republic of");
    map["KR"] = QString::fromUtf8("Korea, Republic of");
    map["KW"] = QString::fromUtf8("Kuwait");
    map["KY"] = QString::fromUtf8("Cayman Islands");
    map["KZ"] = QString::fromUtf8("Kazakhstan");
    map["LA"] = QString::fromUtf8("Lao People's Democratic Republic");
    map["LB"] = QString::fromUtf8("Lebanon");
    map["LC"] = QString::fromUtf8("Saint Lucia");
    map["LI"] = QString::fromUtf8("Liechtenstein");
    map["LK"] = QString::fromUtf8("Sri Lanka");
    map["LR"] = QString::fromUtf8("Liberia");
    map["LS"] = QString::fromUtf8("Lesotho");
    map["LT"] = QString::fromUtf8("Lithuania");
    map["LU"] = QString::fromUtf8("Luxembourg");
    map["LV"] = QString::fromUtf8("Latvia");
    map["LY"] = QString::fromUtf8("Libyan Arab Jamahiriya");
    map["MA"] = QString::fromUtf8("Morocco");
    map["MC"] = QString::fromUtf8("Monaco");
    map["MD"] = QString::fromUtf8("Moldova, Republic of");
    map["MG"] = QString::fromUtf8("Madagascar");
    map["MH"] = QString::fromUtf8("Marshall Islands");
    map["MK"] = QString::fromUtf8("Macedonia, the Former Yugoslav Republic of");
    map["ML"] = QString::fromUtf8("Mali");
    map["MM"] = QString::fromUtf8("Myanmar");
    map["MN"] = QString::fromUtf8("Mongolia");
    map["MO"] = QString::fromUtf8("Macao");
    map["MP"] = QString::fromUtf8("Northern Mariana Islands");
    map["MQ"] = QString::fromUtf8("Martinique");
    map["MR"] = QString::fromUtf8("Mauritania");
    map["MS"] = QString::fromUtf8("Montserrat");
    map["MT"] = QString::fromUtf8("Malta");
    map["MU"] = QString::fromUtf8("Mauritius");
    map["MV"] = QString::fromUtf8("Maldives");
    map["MW"] = QString::fromUtf8("Malawi");
    map["MX"] = QString::fromUtf8("Mexico");
    map["MY"] = QString::fromUtf8("Malaysia");
    map["MZ"] = QString::fromUtf8("Mozambique");
    map["NA"] = QString::fromUtf8("Namibia");
    map["NC"] = QString::fromUtf8("New Caledonia");
    map["NE"] = QString::fromUtf8("Niger");
    map["NF"] = QString::fromUtf8("Norfolk Island");
    map["NG"] = QString::fromUtf8("Nigeria");
    map["NI"] = QString::fromUtf8("Nicaragua");
    map["NL"] = QString::fromUtf8("Netherlands");
    map["NO"] = QString::fromUtf8("Norway");
    map["NP"] = QString::fromUtf8("Nepal");
    map["NR"] = QString::fromUtf8("Nauru");
    map["NU"] = QString::fromUtf8("Niue");
    map["NZ"] = QString::fromUtf8("New Zealand");
    map["OM"] = QString::fromUtf8("Oman");
    map["PA"] = QString::fromUtf8("Panama");
    map["PE"] = QString::fromUtf8("Peru");
    map["PF"] = QString::fromUtf8("French Polynesia");
    map["PG"] = QString::fromUtf8("Papua New Guinea");
    map["PH"] = QString::fromUtf8("Philippines");
    map["PK"] = QString::fromUtf8("Pakistan");
    map["PL"] = QString::fromUtf8("Poland");
    map["PM"] = QString::fromUtf8("Saint Pierre and Miquelon");
    map["PN"] = QString::fromUtf8("Pitcairn");
    map["PR"] = QString::fromUtf8("Puerto Rico");
    map["PS"] = QString::fromUtf8("Palestinian Territory, Occupied");
    map["PT"] = QString::fromUtf8("Portugal");
    map["PW"] = QString::fromUtf8("Palau");
    map["PY"] = QString::fromUtf8("Paraguay");
    map["QA"] = QString::fromUtf8("Qatar");
    map["RE"] = QString::fromUtf8("Reunion");
    map["RO"] = QString::fromUtf8("Romania");
    map["RU"] = QString::fromUtf8("Russian Federation");
    map["RW"] = QString::fromUtf8("Rwanda");
    map["SA"] = QString::fromUtf8("Saudi Arabia");
    map["SB"] = QString::fromUtf8("Solomon Islands");
    map["SC"] = QString::fromUtf8("Seychelles");
    map["SD"] = QString::fromUtf8("Sudan");
    map["SE"] = QString::fromUtf8("Sweden");
    map["SG"] = QString::fromUtf8("Singapore");
    map["SH"] = QString::fromUtf8("Saint Helena");
    map["SI"] = QString::fromUtf8("Slovenia");
    map["SJ"] = QString::fromUtf8("Svalbard and Jan Mayen");
    map["SK"] = QString::fromUtf8("Slovakia");
    map["SL"] = QString::fromUtf8("Sierra Leone");
    map["SM"] = QString::fromUtf8("San Marino");
    map["SN"] = QString::fromUtf8("Senegal");
    map["SO"] = QString::fromUtf8("Somalia");
    map["SR"] = QString::fromUtf8("Suriname");
    map["ST"] = QString::fromUtf8("Sao Tome and Principe");
    map["SV"] = QString::fromUtf8("El Salvador");
    map["SY"] = QString::fromUtf8("Syrian Arab Republic");
    map["SZ"] = QString::fromUtf8("Swaziland");
    map["TC"] = QString::fromUtf8("Turks and Caicos Islands");
    map["TD"] = QString::fromUtf8("Chad");
    map["TF"] = QString::fromUtf8("French Southern Territories");
    map["TG"] = QString::fromUtf8("Togo");
    map["TH"] = QString::fromUtf8("Thailand");
    map["TJ"] = QString::fromUtf8("Tajikistan");
    map["TK"] = QString::fromUtf8("Tokelau");
    map["TL"] = QString::fromUtf8("Timor-Leste");
    map["TM"] = QString::fromUtf8("Turkmenistan");
    map["TN"] = QString::fromUtf8("Tunisia");
    map["TO"] = QString::fromUtf8("Tonga");
    map["TR"] = QString::fromUtf8("Turkey");
    map["TT"] = QString::fromUtf8("Trinidad and Tobago");
    map["TV"] = QString::fromUtf8("Tuvalu");
    map["TW"] = QString::fromUtf8("Taiwan, Province of China");
    map["TZ"] = QString::fromUtf8("Tanzania, United Republic of");
    map["UA"] = QString::fromUtf8("Ukraine");
    map["UG"] = QString::fromUtf8("Uganda");
    map["UM"] = QString::fromUtf8("United States Minor Outlying Islands");
    map["US"] = QString::fromUtf8("United States");
    map["UY"] = QString::fromUtf8("Uruguay");
    map["UZ"] = QString::fromUtf8("Uzbekistan");
    map["VA"] = QString::fromUtf8("Holy See (Vatican City State)");
    map["VC"] = QString::fromUtf8("Saint Vincent and the Grenadines");
    map["VE"] = QString::fromUtf8("Venezuela");
    map["VG"] = QString::fromUtf8("Virgin Islands, British");
    map["VI"] = QString::fromUtf8("Virgin Islands, U.S.");
    map["VN"] = QString::fromUtf8("Vietnam");
    map["VU"] = QString::fromUtf8("Vanuatu");
    map["WF"] = QString::fromUtf8("Wallis and Futuna");
    map["WS"] = QString::fromUtf8("Samoa");
    map["YE"] = QString::fromUtf8("Yemen");
    map["YT"] = QString::fromUtf8("Mayotte");
    map["ZA"] = QString::fromUtf8("South Africa");
    map["ZM"] = QString::fromUtf8("Zambia");
    map["ZW"] = QString::fromUtf8("Zimbabwe");
    return map;
}

static CodeToNameMap gEnglishCountryMap;

/**
 *  \brief Returns a map of ISO-3166 country codes mapped to the country name
 *         in English.
 *  \sa MythContext::GetLanguage()
 */
CodeToNameMap GetISO3166EnglishCountryMap(void)
{
    if (gEnglishCountryMap.empty())
        gEnglishCountryMap = createCountryMap();

    return gEnglishCountryMap;
}

QString GetISO3166EnglishCountryName(QString iso3166Code)
{
    if (gEnglishCountryMap.empty())
        gEnglishCountryMap = createEnglishCountryMap();

    return gEnglishCountryMap[iso3166Code];
}

CodeToNameMap GetISO3166CountryMap()
{
    if (gCountryMap.empty())
        gCountryMap = createCountryMap();

    return gCountryMap;
}

QString GetISO3166CountryName(QString iso3166Code)
{
    if (gCountryMap.empty())
        gCountryMap = createCountryMap();

    return gCountryMap[iso3166Code];
}



