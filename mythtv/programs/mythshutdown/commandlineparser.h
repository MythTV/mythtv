
#include <QString>

#include "mythcommandlineparser.h"

class MythShutdownCommandLineParser : public MythCommandLineParser
{
  public:
    MythShutdownCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

