
#include <QString>

#include "mythcommandlineparser.h"

class MythTVSetupCommandLineParser : public MythCommandLineParser
{
  public:
    MythTVSetupCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

