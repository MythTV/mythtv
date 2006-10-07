#ifndef LANGSETTINGS_H
#define LANGSETTINGS_H

#include "settings.h"

class MPUBLIC LanguageSettings {
public:
    /// Load a QTranslator for the user's preferred
    /// language. The module_name indicates
    /// which .qm file to load.
    static void load(QString module_name);
    
    /// Remove a QTranslator previously installed
    /// using load().
    static void unload(QString module_name);
    
    /// Ask the user for the language to use. If a
    /// language was already specified at the last
    /// load(), it will not ask unless force is set.
    static void prompt(bool force = false);
    
    /// Reload all active translators based on the
    /// current language setting. Call this after changing
    /// the language pref in the database.
    static void reload();
    
    /// List the languages supported by Myth.
    /// The list will always be an even number of items;
    /// the language name precedes its language code.
    static QStringList getLanguages();
    
    /// Fill out a settings widget with the supported
    /// language settings. Convenience wrapper around
    /// getLanguages() functionality.
    static void fillSelections(SelectSetting *widget);

protected:
    static class LanguageSettingsPrivate d;
};

#endif
