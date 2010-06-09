#ifndef _MYTHFONTMANAGER_H
#define _MYTHFONTMANAGER_H

#include <QMultiHash>
#include <QMutex>
#include <QString>

#include "mythexp.h"

class MythFontReference;
typedef QMultiHash<QString, MythFontReference*> FontPathToReference;

class MPUBLIC MythFontManager
{
  public:
    MythFontManager() {}

    void LoadFonts(const QString &directory, const QString &registeredFor);
    void ReleaseFonts(const QString &registeredFor);

    static MythFontManager *GetGlobalFontManager(void);

  private:
    void LoadFonts(const QString &directory, const QString &registeredFor,
                   int *maxDirs);
    void LoadFontsFromDirectory(const QString &directory,
                                const QString &registeredFor);
    void LoadFontFile(const QString &fontPath, const QString &registeredFor);
    bool RegisterFont(const QString &fontPath, const QString &registeredFor,
                      const int fontID = -1);
    bool IsFontFileLoaded(const QString &fontPath);

    QMutex m_lock;
    FontPathToReference m_fontPathToReference;

};

MPUBLIC MythFontManager *GetGlobalFontManager(void);

class MythFontReference
{
  public:
    MythFontReference(const QString &fontPath, const QString &registeredFor,
                      const int fontID);

    QString GetFontPath(void) const { return m_fontPath; }
    QString GetRegisteredFor(void) const { return m_registeredFor; }
    int GetFontID(void) const { return m_fontID; }

  private:
    const QString m_fontPath;
    const QString m_registeredFor;
    const int m_fontID;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
