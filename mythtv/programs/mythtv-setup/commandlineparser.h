
#include <QString>

#include "mythcommandlineparser.h"

class MythTVSetupCommandLineParser : public MythCommandLineParser
{
  public:
    MythTVSetupCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

