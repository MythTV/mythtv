
#include <QString>

#include "mythcommandlineparser.h"

class MythFileRecorderCommandLineParser : public MythCommandLineParser
{
  public:
    MythFileRecorderCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

