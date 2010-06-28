#include "langsettings.h"

#include <QTranslator>
#include <QDir>
#include <QFileInfo>
#include <QApplication>

#include "mythcorecontext.h"
#include "mythstorage.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "mythlocale.h"

// LangEditor provides the GUI for the prompt() routine.
#include <mythscreentype.h>

class LangEditorSetting : public ListBoxSetting, public Storage
{
  public:
    LangEditorSetting() : ListBoxSetting(this)
    {
        setLabel(QObject::tr("Select your preferred language"));
    };

    virtual void Load(void)
    {
        LanguageSettings::fillSelections(this);
    }

    virtual void Save(void)
    {
        gCoreContext->SetSetting("Language", getValue());
        gCoreContext->SaveSetting("Language", getValue());
        LanguageSettings::reload();
    }

    virtual void Save(QString /*destination*/) { }
};

// LanguageSettingsPrivate holds our persistent data.
// It's a singleton class, instantiated in the static
// member variable d of LanguageSettings.

typedef QMap<QString, QTranslator*> TransMap;

class LanguageSettingsPrivate
{
  public:
    LanguageSettingsPrivate():
        m_loaded(false),
        m_language("")    { };

    void Init(void)
    {
        if (!m_loaded)
        {
            m_loaded = "loaded";
            m_language = gCoreContext->GetSetting("Language");
        }
    };

    bool LanguageChanged(void)
    {
        QString cur_language = gCoreContext->GetSetting("Language");
        bool ret = false;
        if (!cur_language.isEmpty() &&
            cur_language.compare(m_language))
            ret = true;
        m_language = cur_language;
        return ret;
    };

    bool m_loaded;
    QString m_language;
    TransMap m_translators;
 };

LanguageSettingsPrivate LanguageSettings::d;

void LanguageSettings::load(QString module_name)
{
    d.Init();
    if (!d.m_language.isEmpty())
    {
        // unload any previous version
        unload(module_name);

        // install translator
        QString      lang  = d.m_language.toLower();

        if (lang == "en")
        {
            gCoreContext->SetSetting("Language", "EN_US");
            gCoreContext->SaveSetting("Language", "EN_US");
            lang = "en_us";
        }

        QTranslator *trans = new QTranslator(0);
        if (trans->load(GetTranslationsDir() + module_name
                        + "_" + lang + ".qm", "."))
        {
            qApp->installTranslator(trans);
            d.m_translators[module_name] = trans;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Cannot load language " + lang
                                  + " for module " + module_name);
        }
    }
}

void LanguageSettings::unload(QString module_name)
{
    TransMap::Iterator it = d.m_translators.find(module_name);
    if (it != d.m_translators.end())
    {
        // found translator, remove it from qApp and our map
        qApp->removeTranslator(*it);
        delete *it;
        d.m_translators.erase(it);
    }
}

void LanguageSettings::prompt(bool force)
{
    d.Init();
    // Ask for language if we don't already know.
    if (force || d.m_language.isEmpty())
    {
        ConfigurationDialog langEdit;
        langEdit.addChild(new LangEditorSetting());
        langEdit.exec();
    }
    // Always update the database, even if there's
    // no change -- during bootstrapping, we don't
    // actually get to write to the database until
    // a later run, so do it every time.
    gCoreContext->SaveSetting("Language", d.m_language);
}

void LanguageSettings::reload(void)
{
    // Update our translators if necessary.
    // We need two loops, as the QMap wasn't happy with
    // me changing its contents during my iteration.
    if (d.LanguageChanged())
    {
        QStringList keys;
        for (TransMap::Iterator it = d.m_translators.begin();
             it != d.m_translators.end();
             ++it)
            keys.append(it.key());

        for (QStringList::Iterator it = keys.begin();
             it != keys.end();
             ++it)
            load(*it);
    }
}

QStringList LanguageSettings::getLanguages(void)
{
    QStringList langs;

    QDir translationDir(GetTranslationsDir());
    translationDir.setNameFilters(QStringList("mythfrontend_*.qm"));
    translationDir.setFilter(QDir::Files);
    QFileInfoList translationFiles = translationDir.entryInfoList();
    QFileInfoList::const_iterator it;
    for (it = translationFiles.constBegin(); it != translationFiles.constEnd();
         ++it)
    {
        // We write the names incorrectly as all lowercase, so fix this before
        // sending to QLocale
        QString languageCode = (*it).baseName().section('_', 1, 1);
        QString countryCode = (*it).baseName().section('_', 2, 2);
        if (!countryCode.isEmpty())
            languageCode = QString("%1_%2").arg(languageCode)
                                           .arg(countryCode.toUpper());

        MythLocale locale(languageCode);
        QString language = locale.GetNativeLanguage();
        if (language.isEmpty())
            language = locale.GetLanguage(); // Fall back to English

        if (!countryCode.isEmpty())
        {
            QString country = locale.GetNativeCountry();
            if (country.isEmpty())
                country = locale.GetCountry(); // Fall back to English

            language.append(QString(" (%1)").arg(country));
        }

        langs.append(language);
        langs.append(languageCode);
    }

    return langs;
}

void LanguageSettings::fillSelections(SelectSetting *widget)
{
    QStringList langs = LanguageSettings::getLanguages();
    QString langCode = gCoreContext->GetLocale()->GetLanguageCode();
    if (langCode.isEmpty())
        langCode = "en_us";
    widget->clearSelections();
    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
    {
        QString label = *it;
        QString value = *(++it);
        widget->addSelection(label, value, (value == langCode));
    }
}

