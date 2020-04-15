#ifndef MYTHFONTMANAGER_H
#define MYTHFONTMANAGER_H

#include <utility>

// Qt headers
#include <QMultiHash>
#include <QMutex>
#include <QString>

// MythTV headers
#include "mythuiexp.h"

class MythFontReference;
using FontPathToReference = QMultiHash<QString, MythFontReference*>;

class MUI_PUBLIC MythFontManager
{
  public:
    MythFontManager() = default;

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
                      int fontID = -1);
    bool IsFontFileLoaded(const QString &fontPath);

    QMutex m_lock;
    FontPathToReference m_fontPathToReference;

};

MUI_PUBLIC MythFontManager *GetGlobalFontManager(void);

class MythFontReference
{
  public:
    MythFontReference(QString fontPath, QString registeredFor,
                      const int fontID)
        : m_fontPath(std::move(fontPath)),
          m_registeredFor(std::move(registeredFor)),
          m_fontID(fontID) {}

    QString GetFontPath(void) const { return m_fontPath; }
    QString GetRegisteredFor(void) const { return m_registeredFor; }
    int GetFontID(void) const { return m_fontID; }

  private:
    const QString m_fontPath;
    const QString m_registeredFor;
    const int m_fontID;
};

#endif // MYTHFONTMANAGER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
