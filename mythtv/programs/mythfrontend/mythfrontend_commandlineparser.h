
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythFrontendCommandLineParser : public MythCommandLineParser
{
  public:
    MythFrontendCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

