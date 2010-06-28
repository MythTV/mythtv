
#include "mythlocale.h"

// QT
#include <QLocale>
#include <QDomDocument>
#include <QFile>
#include <QIODevice>

// Libmythdb
#include "mythverbose.h"
#include "mythdb.h"
#include "mythdirs.h"


typedef QMap<QString, QString> CodeToNameMap;

static CodeToNameMap createNativeLangMap(void)
{
    CodeToNameMap map;
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

static CodeToNameMap gNativeLangMap;

static CodeToNameMap createNativeCountryMap(void)
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

static CodeToNameMap gNativeCountryMap;


MythLocale::MythLocale(QString localeName)
{
    QLocale locale;

    if (!localeName.isEmpty())
        locale = QLocale(localeName);
    else
        locale = QLocale::system();

    m_localeCode = locale.name();
    m_country = locale.country();
    m_language = locale.language();

    if (m_localeCode.isEmpty())
        m_localeCode = "en_us";
}

QString MythLocale::GetCountryCode(void) const
{
    QString isoCountry = m_localeCode.section("_", 1, 1);

    return isoCountry;
}

QString MythLocale::GetNativeCountry(void) const
{
    if (gNativeCountryMap.isEmpty())
        gNativeCountryMap = createNativeCountryMap();

    return gNativeCountryMap[GetCountryCode()];
}

QString MythLocale::GetLanguageCode(void) const
{
    QString isoLanguage = m_localeCode.section("_", 0, 0);

    return isoLanguage;
}

QString MythLocale::GetNativeLanguage(void) const
{
    if (gNativeLangMap.isEmpty())
        gNativeLangMap = createNativeLangMap();

    return gNativeLangMap[GetLanguageCode()];
}

bool MythLocale::LoadDefaultsFromXML(void)
{
    m_globalSettings.clear();
    QDomDocument doc;

    QString path = QString("/locales/%1.xml").arg(m_localeCode.toLower());

    QFile file(path.prepend(GetShareDir()));
    if (!file.exists())
    {
        file.setFileName(path.prepend(GetConfDir()));

        if (!file.exists())
        {
            VERBOSE(VB_GENERAL, QString("No locale defaults file for %1, "
                                        "skipping").arg(m_localeCode));
            return false;
        }
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to open %1")
                                                        .arg(file.fileName()));
        return false;
    }

    VERBOSE(VB_GENERAL, QString("Reading locale defaults from %1")
                                                        .arg(file.fileName()));

    if (!doc.setContent(&file))
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to parse %1")
                                                        .arg(file.fileName()));

        file.close();
        return false;
    }
    file.close();

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "setting")
            {
                QString name = e.attribute("name", "");
                bool global = (e.attribute("global", "false") == "true");
                QString value = e.firstChild().toText().data();

                // TODO Assumes no setting accepts an empty value, which may not
                // be the case
                if (!name.isEmpty() && !value.isEmpty())
                {
                    if (global)
                        m_globalSettings[name] = value;
                    else
                        m_hostSettings[name] = value;
                }
            }
        }
    }

    if (m_globalSettings.isEmpty() && m_hostSettings.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("No locale defaults specified in %1, "
                                    "skipping").arg(file.fileName()));
        return false;
    }

    return true;
}

void MythLocale::SaveLocaleDefaults(bool overwrite)
{
    if (!LoadDefaultsFromXML())
        return;

    SettingsMap::iterator it;
    for (it = m_globalSettings.begin(); it != m_globalSettings.end(); ++it)
    {
        MythDB *mythDB = MythDB::getMythDB();
        if (overwrite || mythDB->GetSetting(it.key()).isEmpty())
        {
            mythDB->SetSetting(it.key(), it.value());
            mythDB->SaveSettingOnHost(it.key(), it.value(), "");
        }
    }

    for (it = m_hostSettings.begin(); it != m_hostSettings.end(); ++it)
    {
        MythDB *mythDB = MythDB::getMythDB();
        if (overwrite || mythDB->GetSetting(it.key()).isEmpty())
        {
            mythDB->SetSetting(it.key(), it.value());
            mythDB->SaveSetting(it.key(), it.value());
        }
    }
}

void MythLocale::ResetToLocaleDefaults(void)
{
    SaveLocaleDefaults(true);
}

void MythLocale::ResetToStandardDefaults(void)
{
    // TODO Not implemented yet, delete everything in m_globalSettings
    //      from the database then let the standard defaults populate them
    //      again. Used if the user wants to revert the changes
    return;
}

