#include <QCoreApplication>
#include <QTranslator>

#include "langsettings.h"
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythverbose.h"

// LangEditor provides the GUI for the prompt() routine.

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

class LanguageSettingsPrivate {
public:
    LanguageSettingsPrivate():
        m_loaded(false),
        m_language("")    { };

    void Init(void) {
        if (!m_loaded)
        {
            m_loaded = "loaded";
            m_language = gCoreContext->GetSetting("Language");
        }
    };

    bool LanguageChanged(void) {
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
    langs << QString::fromUtf8("English (US)")<< "EN_US"   // English
          << QString::fromUtf8("Italiano")    << "IT"   // Italian
          << QString::fromUtf8("Català")
              << "CA"                                   // Catalan
          << QString::fromUtf8("Español")
              << "ES"                                   // Spanish
          << QString::fromUtf8("Nederlands")  << "NL"   // Dutch
          << QString::fromUtf8("Français")
              << "FR"                                   // French
          << QString::fromUtf8("Deutsch")     << "DE"   // German
          << QString::fromUtf8("Dansk")       << "DA"   // Danish
          << QString::fromUtf8("Islenska")    << "IS"   // Icelandic
          << QString::fromUtf8("Norsk (bokmål)")
              << "NB"                                   // Norwegian (bokmal)
          << QString::fromUtf8("Svenska")     << "SV"   // Swedish
          << QString::fromUtf8("Polski")      << "PL"   // Polish
          << QString::fromUtf8("Português")
              << "PT"                                   // Portuguese
          << QString::fromUtf8("Nihongo")     << "JA"   // Japanese
          << QString::fromUtf8("Slovenski")   << "SL"   // Slovenian
          << QString::fromUtf8("Suomi")       << "FI"   // Finnish
          << QString::fromUtf8("Hanzi (Traditional)")
              << "ZH_TW"                                // Traditional Chinese
          << QString::fromUtf8("Eesti")       << "ET"   // Estonian
          << QString::fromUtf8("Português Brasileiro")
              << "PT_BR"                                // Brazilian Portuguese
          << QString::fromUtf8("English (British)")
              << "EN_GB"                                // British English
          << QString::fromUtf8("Česky")
              << "CS"                                   // Czech
          << QString::fromUtf8("Türkçe")
              << "TR"                                   // Turkish
          << QString::fromUtf8("Русский")
              << "RU"                                   // Russian
          << QString::fromUtf8("עברית")
              << "HE"                                   // Hebrew
          << QString::fromUtf8("العربية")
              << "AR"                                   // Arabic
          << QString::fromUtf8("Hrvatski")    << "HR"   // Croatian
          << QString::fromUtf8("Magyar")    << "HU"   // Hungarian
          << QString::fromUtf8("Ελληνικά")    << "EL"   // Hellenic
          << QString::fromUtf8("Български")   << "BG"   // Bulgarian
          ;
    return langs;
}

void LanguageSettings::fillSelections(SelectSetting *widget)
{
    QStringList langs = LanguageSettings::getLanguages();
    widget->clearSelections();
    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
    {
        QString label = *it;
        QString value = *(++it);
        widget->addSelection(label, value);
    }
}

