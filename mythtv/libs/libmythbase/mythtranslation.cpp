
#include "mythtranslation.h"

// QT
#include <QStringList>
#include <QFileInfo>
#include <QApplication>
#include <QDir>

// libmythbase
#include "mythdirs.h"
#include "mythcorecontext.h"
#include "mythlogging.h"

using TransMap = QMap<QString, QTranslator*>;

class MythTranslationPrivate
{
  public:
    MythTranslationPrivate() = default;

    QString m_language;
    TransMap m_translators;
};

MythTranslationPrivate MythTranslation::d;

void MythTranslation::load(const QString &module_name)
{
    // update m_language via LanguageChanged() and load the new translation
    // if one was already loaded and the language has changed
    reload();

    if (!d.m_translators.contains(module_name))
    {
        load_real(module_name);
    }
}

void MythTranslation::load_real(const QString &module_name)
{
    // unload any previous version
    unload(module_name);

    // install translator
    QString lang = d.m_language.toLower();

    if (d.m_language.isEmpty())
    {
        lang = "en_us";
    }

    if (lang == "en")
    {
        gCoreContext->OverrideSettingForSession("Language", "en_US");
        lang = "en_us";
    }

    auto *trans = new QTranslator(nullptr);
    if (trans->load(GetTranslationsDir() + module_name
                    + "_" + lang + ".qm", "."))
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Loading %1 translation for module %2")
                .arg(lang, module_name));
        QCoreApplication::installTranslator(trans);
        d.m_translators[module_name] = trans;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error Loading %1 translation for "
                                         "module %2").arg(lang, module_name));
    }
}

void MythTranslation::unload(const QString &module_name)
{
    TransMap::Iterator it = d.m_translators.find(module_name);
    if (it != d.m_translators.end())
    {
        // found translator, remove it from qApp and our map
        QCoreApplication::removeTranslator(*it);
        delete *it;
        d.m_translators.erase(it);
    }
}

bool MythTranslation::LanguageChanged(void)
{
    QString currentLanguage = gCoreContext->GetSetting("Language");
    bool ret = false;
    if (!currentLanguage.isEmpty() && currentLanguage.compare(d.m_language))
        ret = true;
    d.m_language = currentLanguage;
    return ret;
}

void MythTranslation::reload()
{
    // Update our translators if necessary.
    // We need two loops, as the QMap wasn't happy with
    // me changing its contents during my iteration.
    if (LanguageChanged())
    {
        QStringList keys;
        for (TransMap::Iterator it = d.m_translators.begin();
             it != d.m_translators.end();
             ++it)
            keys.append(it.key());

        for (const auto& key : std::as_const(keys))
            load_real(key);
    }
}

QMap<QString, QString> MythTranslation::getLanguages(void)
{
    QMap<QString, QString> langs;

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
            languageCode = QString("%1_%2")
                .arg(languageCode, countryCode.toUpper());

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

        langs[languageCode] = language;
    }

    return langs;
}
