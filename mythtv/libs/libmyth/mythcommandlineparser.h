// -*- Mode: c++ -*-

#include <QString>
#include <QMap>

#include "mythexp.h"

typedef enum {
    kCLPOverrideSettingsFile = 0x00000001,
    kCLPOverrideSettings     = 0x00000002,
    kCLPWindowed             = 0x00000004,
    kCLPNoWindowed           = 0x00000008,
} ParseType;

class MPUBLIC MythCommandLineParser
{
  public:
    MythCommandLineParser(uint64_t things_to_parse);

    bool Parse(int argc, const char * const * argv, int &argpos, bool &err);
    QString GetHelpString(bool with_header) const;

    QMap<QString,QString> GetSettingsOverride(void) const
        { return settingsOverride; }

  private:
    uint64_t              parseTypes;
    QMap<QString,QString> settingsOverride;
};
