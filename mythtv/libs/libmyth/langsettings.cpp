
#include "langsettings.h"

// qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QApplication>

// libmythdb
#include "mythcorecontext.h"
#include "mythstorage.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "mythlocale.h"
#include "mythtranslation.h"

// libmythui
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythmainwindow.h"

LanguageSelection::LanguageSelection(MythScreenStack *parent, bool exitOnFinish)
                 :MythScreenType(parent, "LanguageSelection"),
                  m_exitOnFinish(exitOnFinish), m_loaded(false)
{
    m_language = gCoreContext->GetSetting("Language");
    m_country = gCoreContext->GetSetting("Country");
}

LanguageSelection::~LanguageSelection()
{
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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'languageselection'");
        return false;
    }

//     connect(m_countryList, SIGNAL(itemClicked(MythUIButtonListItem*)),
//             SLOT(LocaleClicked(MythUIButtonListItem*)));
//     connect(m_languageList, SIGNAL(itemClicked(MythUIButtonListItem*)),
//             SLOT(LanguageClicked(MythUIButtonListItem*)));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    m_languageList->SetLCDTitles(tr("Preferred language"), "");
    m_countryList->SetLCDTitles(tr("Your location"), "");

    BuildFocusList();

    return true;
}

void LanguageSelection::Load(void)
{
    QString langCode = gCoreContext->GetLocale()->GetLanguageCode();
    QString localeCode = gCoreContext->GetLocale()->GetLocaleCode();
    QString countryCode = gCoreContext->GetLocale()->GetCountryCode();

    VERBOSE(VB_GENERAL, QString("System Locale (%1), Country (%2), Language "
                                "(%3)").arg(localeCode).arg(countryCode)
                                .arg(langCode));

    CodeToNameMap langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();
    MythUIButtonListItem *item;
    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
    {
        QString nativeLang = *it;
        QString code = langMap.key(nativeLang); // Slow, but map is small
        item = new MythUIButtonListItem(m_languageList, nativeLang);
        item->SetText(nativeLang, "nativelanguage");
        item->SetData(code);

         // We have to compare against locale for languages like en_GB
        if (code == m_language || code == langCode || code == localeCode)
            m_languageList->SetItemCurrent(item);
    }

    CodeToNameMap localesMap = GetISO3166EnglishCountryMap();
    QStringList locales = localesMap.values();
    locales.sort();
    for (QStringList::Iterator it = locales.begin(); it != locales.end();
         ++it)
    {
        QString country = *it;
        QString code = localesMap.key(country); // Slow, but map is small
        QString nativeCountry = GetISO3166CountryName(code);
        item = new MythUIButtonListItem(m_countryList, country);
        item->SetData(code);
        item->SetText(country, "country");
        item->SetText(nativeCountry, "nativecountry");
        item->SetImage(QString("locale/%1.png").arg(code.toLower()));

        if (code == m_country || code == countryCode)
            m_countryList->SetItemCurrent(item);
    }
}

bool LanguageSelection::m_languageChanged = false;

bool LanguageSelection::prompt(bool force)
{
    m_languageChanged = false;
    QString language = gCoreContext->GetSetting("Language", "");
    // Ask for language if we don't already know.
    if (force || language.isEmpty())
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        if (!mainStack)
            return false;

        LanguageSelection *langSettings = new LanguageSelection(mainStack,
                                                                true);

        if (langSettings->Create())
        {
            mainStack->AddScreen(langSettings, false);
            qApp->exec();
            mainStack->PopScreen(langSettings, false);
        }
        else
            delete langSettings;
    }

    return m_languageChanged;
}

void LanguageSelection::Save(void)
{
    MythUIButtonListItem *item = m_languageList->GetItemCurrent();

    QString langCode = item->GetData().toString();
    gCoreContext->SetSetting("Language", langCode);
    gCoreContext->SaveSetting("Language", langCode);

    item = m_countryList->GetItemCurrent();

    QString countryCode = item->GetData().toString();
    gCoreContext->SetSetting("Country", countryCode);
    gCoreContext->SaveSetting("Country", countryCode);

    if (m_language != langCode)
        m_languageChanged = true;

    Close();
}

void LanguageSelection::Close(void)
{
    if (m_exitOnFinish)
        qApp->quit();
    else
        MythScreenType::Close();
}
