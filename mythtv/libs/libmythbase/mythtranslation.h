#ifndef MYTHTRANSLATION_H_
#define MYTHTRANSLATION_H_

#include <QTranslator>
#include <QString>
#include <QMap>

#include "mythbaseexp.h"

class MBASE_PUBLIC MythTranslation
{

  public:
    static QMap<QString, QString> getLanguages(void);

    /// Load a QTranslator for the user's preferred
    /// language. The module_name indicates
    /// which .qm file to load.
    static void load(const QString &module_name);

    /// Remove a QTranslator previously installed
    /// using load().
    static void unload(const QString &module_name);

    /// Reload all active translators based on the
    /// current language setting. Call this after changing
    /// the language pref in the database.
    static void reload();

    static bool LanguageChanged(void);

  protected:
    static void load_real(const QString &module_name);

    static class MythTranslationPrivate d;
};

#endif
