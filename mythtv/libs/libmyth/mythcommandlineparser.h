// -*- Mode: c++ -*-

#include <QStringList>
#include <QMap>

#include <stdint.h>   // for uint64_t

#include "mythexp.h"

typedef enum {
    kCLPOverrideSettingsFile = 0x00000001,
    kCLPOverrideSettings     = 0x00000002,
    kCLPWindowed             = 0x00000004,
    kCLPNoWindowed           = 0x00000008,
    kCLPGetSettings          = 0x00000010,
    kCLPQueryVersion         = 0x00000020,
    kCLPDisplay              = 0x00000040,
    kCLPGeometry             = 0x00000080,
    kCLPVerbose              = 0x00000100,
    kCLPHelp                 = 0x00000200,
    kCLPExtra                = 0x00000400,
} ParseType;

class MPUBLIC MythCommandLineParser
{
  public:
    MythCommandLineParser(int things_to_parse);

    bool PreParse(int argc, const char * const * argv, int &argpos, bool &err);
    bool Parse(int argc, const char * const * argv, int &argpos, bool &err);
    QString GetHelpString(bool with_header) const;

    QMap<QString,QString> GetSettingsOverride(void) const
        { return settingsOverride; }
    QStringList GetSettingsQuery(void) const
        { return settingsQuery; }
    QString GetDisplay(void)  const { return display;     }
    QString GetGeometry(void) const { return geometry;    }

    bool    WantsToExit(void) const { return wantsToExit; }

  private:
    int                   parseTypes;

    QMap<QString,QString> settingsOverride;
    QStringList           settingsQuery;
    QString               display;
    QString               geometry;

    bool                  wantsToExit;
};
