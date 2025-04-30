
#include "langsettings.h"

// qt
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>

// libmythbase
#include "libmythbase/iso3166.h"
#include "libmythbase/iso639.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlocale.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythstorage.h"
#include "libmythbase/mythtranslation.h"

// libmythui
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"

LanguageSelection::LanguageSelection(MythScreenStack *parent, bool exitOnFinish)
                 :MythScreenType(parent, "LanguageSelection"),
                  m_exitOnFinish(exitOnFinish),
                  m_language(gCoreContext->GetSetting("Language")),
                  m_country(gCoreContext->GetSetting("Country"))
{
    if (exitOnFinish)
    {
        m_loop = new QEventLoop();
    }
}

LanguageSelection::~LanguageSelection()
{
    if (m_exitOnFinish)
    {
        delete m_loop;
    }
}

bool LanguageSelection::Create(void)
{
    if (!LoadWindowFromXML("config-ui.xml", "languageselection", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_languageList, "languages", &err);
    UIUtilE::Assign(this, m_countryList, "countries", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "Cannot load screen 'languageselection'");
        return false;
    }

#if 0
    connect(m_countryList, &MythUIButton::Clicked, this, &LanguageSelection::LocaleClicked);
    connect(m_languageList, &MythUIButton::Clicked, this, &LanguageSelection::LanguageClicked);
#endif

    connect(m_saveButton, &MythUIButton::Clicked, this, &LanguageSelection::Save);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &LanguageSelection::Close);

    m_languageList->SetLCDTitles(tr("Preferred language"), "");
    m_countryList->SetLCDTitles(tr("Your location"), "");

    BuildFocusList();

    return true;
}

void LanguageSelection::Load(void)
{
    auto *locale = new MythLocale();

    QString langCode;

    if (gCoreContext->GetLocale())
    {
        // If the global MythLocale instance exists, then we should use it
        // since it's informed by previously chosen values from the
        // database.
        *locale = *gCoreContext->GetLocale();
    }
    else
    {
        // If no global MythLocale instance exists then we're probably
        // bootstrapping before the database is available, in that case
        // we want to load language from the locale XML defaults if they
        // exist.
        // e.g. the locale defaults might define en_GB for Australia which has
        // no translation of it's own. We can't automatically derive en_GB
        // from en_AU which MythLocale will arrive at and there is no 'en'
        // translation.
        langCode = locale->GetLocaleSetting("Language");
    }

    if (langCode.isEmpty())
        langCode = locale->GetLanguageCode();
    QString localeCode = locale->GetLocaleCode();
    QString countryCode = locale->GetCountryCode();

    LOG(VB_GENERAL, LOG_INFO,
             QString("System Locale (%1), Country (%2), Language (%3)")
                     .arg(localeCode, countryCode, langCode));

    QMap<QString,QString> langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();
    bool foundLanguage = false;
    for (const auto& nativeLang : std::as_const(langs))
    {
        QString code = langMap.key(nativeLang); // Slow, but map is small
        QString language = GetISO639EnglishLanguageName(code);
        auto *item = new MythUIButtonListItem(m_languageList, nativeLang);
        item->SetText(language, "language");
        item->SetText(nativeLang, "nativelanguage");
        item->SetData(code);

         // We have to compare against locale for languages like en_GB
        if (code.toLower() == m_language.toLower() ||
            code == langCode || code == localeCode)
        {
            m_languageList->SetItemCurrent(item);
            foundLanguage = true;
        }
    }

    if (m_languageList->IsEmpty())
    {
        LOG(VB_GUI, LOG_ERR, "ERROR - Failed to load translations, at least "
                             "one translation file MUST be installed.");

        auto *item = new MythUIButtonListItem(m_languageList,
                                              "English (United States)");
        item->SetText("English (United States)", "language");
        item->SetText("English (United States)", "nativelanguage");
        item->SetData("en_US");
    }

    if (!foundLanguage)
        m_languageList->SetValueByData("en_US");

    ISO3166ToNameMap localesMap = GetISO3166EnglishCountryMap();
    QStringList locales = localesMap.values();
    locales.sort();
    for (const auto& country : std::as_const(locales))
    {
        QString code = localesMap.key(country); // Slow, but map is small
        QString nativeCountry = GetISO3166CountryName(code);
        auto *item = new MythUIButtonListItem(m_countryList, country);
        item->SetData(code);
        item->SetText(country, "country");
        item->SetText(nativeCountry, "nativecountry");
        item->SetImage(QString("locale/%1.png").arg(code.toLower()));

        if (code == m_country || code == countryCode)
            m_countryList->SetItemCurrent(item);
    }

    delete locale;
}

bool LanguageSelection::m_languageChanged = false;

bool LanguageSelection::prompt(bool force)
{
    m_languageChanged = false;
    QString language = gCoreContext->GetSetting("Language", "");
    QString country = gCoreContext->GetSetting("Country", "");
    // Ask for language if we don't already know.
    if (force || language.isEmpty() || country.isEmpty())
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        if (!mainStack)
            return false;

        auto *langSettings = new LanguageSelection(mainStack, true);

        if (langSettings->Create())
        {
            mainStack->AddScreen(langSettings, false);
            langSettings->m_loop->exec();
            mainStack->PopScreen(langSettings, false);
        }
        else
        {
            delete langSettings;
        }
    }

    return m_languageChanged;
}

void LanguageSelection::Save(void)
{
    MythUIButtonListItem *item = m_languageList->GetItemCurrent();

    if (!item)
    {
        Close();
        LOG(VB_GUI, LOG_ERR,
                 "LanguageSelection::Save called without current languageList");
        return;
    }

    QString langCode = item->GetData().toString();
    gCoreContext->SaveSetting("Language", langCode);

    item = m_countryList->GetItemCurrent();

    if (!item)
    {
        Close();
        LOG(VB_GUI, LOG_ERR,
                 "LanguageSelection::Save called without current countryList");
        return;
    }

    QString countryCode = item->GetData().toString();
    gCoreContext->SaveSetting("Country", countryCode);

    if (m_language != langCode)
        m_languageChanged = true;

    Close();
}

void LanguageSelection::Close(void)
{
    if (m_exitOnFinish)
        m_loop->quit();
    else
        MythScreenType::Close();
}
