#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"

class GlobalSetting: public SimpleDBStorage, virtual public Configurable {
public:
    GlobalSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1' AND hostname = '%2'")
                       .arg(getName()).arg(gContext->GetHostName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2', hostname = '%3'")
                       .arg(getName()).arg(getValue())
                       .arg(gContext->GetHostName());
    };
};

class BackendSetting: public SimpleDBStorage, virtual public Configurable {
public:
    BackendSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1'").arg(getName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2'").arg(getName()).arg(getValue());
    };
};

class ThemeSelector: public ImageSelectSetting, public GlobalSetting {
public:
    ThemeSelector();
};

// The real work.

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings();
};

class GeneralSettings: virtual public ConfigurationWizard {
public:
    GeneralSettings();
};

class EPGSettings: virtual public ConfigurationWizard {
public:
    EPGSettings();
};

class AppearanceSettings: virtual public ConfigurationWizard {
public:
    AppearanceSettings();
};

class MainGeneralSettings: virtual public ConfigurationWizard {
public:
    MainGeneralSettings();
};

#endif
