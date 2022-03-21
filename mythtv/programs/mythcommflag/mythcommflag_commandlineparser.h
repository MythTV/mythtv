
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythCommFlagCommandLineParser : public MythCommandLineParser
{
  public:
    MythCommFlagCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

