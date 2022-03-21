
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythLCDServerCommandLineParser : public MythCommandLineParser
{
  public:
    MythLCDServerCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

