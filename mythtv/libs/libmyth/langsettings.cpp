#include <qapplication.h>
#include <qsqldatabase.h>

using namespace std;

#include "langsettings.h"
#include "settings.h"
#include "mythcontext.h"


// LangEditor provides the GUI for the prompt() routine.

class LangEditor: public ListBoxSetting, public ConfigurationDialog {
public:
    LangEditor() {
        setLabel(QObject::tr("Select your preferred language"));
    };
    
    virtual void load() {
        LanguageSettings::fillSelections(this);
    };
    
    virtual void save() {
        gContext->SetSetting("Language", getValue());
        gContext->SaveSetting("Language", getValue());
        LanguageSettings::reload();
    };
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
            m_language = gContext->GetSetting("Language");
        }
    };
    
    bool LanguageChanged(void) {
        QString cur_language = gContext->GetSetting("Language");
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
        QTranslator *trans = new QTranslator(0);
        trans->load(gContext->GetTranslationsDir() +
                    module_name + QString("_") +
                    d.m_language.lower() + QString(".qm"), ".");
         qApp->installTranslator(trans);
         d.m_translators[module_name] = trans;
    }
}

void LanguageSettings::unload(QString module_name)
{
    TransMap::Iterator it = d.m_translators.find(module_name);
    if (it != d.m_translators.end())
    {
        // found translator, remove it from qApp and our map
        qApp->removeTranslator(it.data());
        delete it.data();
        d.m_translators.remove(it);
    }
}

void LanguageSettings::prompt(bool force)
{
    d.Init();
    // Ask for language if we don't already know.
    if (force || d.m_language.isEmpty())
    {
        LangEditor *ed = new LangEditor();
        ed->exec();
        delete ed;
    }
    // Always update the database, even if there's
    // no change -- during bootstrapping, we don't
    // actually get to write to the database until
    // a later run, so do it every time.
    gContext->SaveSetting("Language", d.m_language);
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
    langs << QString::fromUtf8("English")     << "EN"   // English
          << QString::fromUtf8("Italiano")    << "IT"   // Italian
          << QString::fromUtf8("Català")      << "CA"   // Catalan
          << QString::fromUtf8("Español")     << "ES"   // Spanish
          << QString::fromUtf8("Nederlands")  << "NL"   // Dutch
          << QString::fromUtf8("Français")    << "FR"   // French
          << QString::fromUtf8("Deutsch")     << "DE"   // German
          << QString::fromUtf8("Dansk")       << "DK"   // Danish
          << QString::fromUtf8("Svenska")     << "SV"   // Swedish
          << QString::fromUtf8("Português")   << "PT"   // Portuguese
       // << QString::fromUtf8("日本語")   << "JA"   // Japanese
          << QString::fromUtf8("Nihongo")     << "JA"   // Japanese
          << QString::fromUtf8("Slovenski")   << "SI"   // Slovenian
          << QString::fromUtf8("Suomi")       << "FI"   // Finnish
          << QString::fromUtf8("Hanzi (Traditional)")
              << "ZH_TW";                               // Traditional Chinese
    return langs;
}

void LanguageSettings::fillSelections(SelectSetting *widget)
{
    QStringList langs = LanguageSettings::getLanguages();
    widget->clearSelections();
    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
    {
        widget->addSelection(*it, *(++it));
    }
}

