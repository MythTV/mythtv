
#include <QString>

#include "mythcommandlineparser.h"

class MythWelcomeCommandLineParser : public MythCommandLineParser
{
  public:
    MythWelcomeCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

