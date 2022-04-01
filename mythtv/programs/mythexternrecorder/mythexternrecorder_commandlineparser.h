
#include <QString>

#include "mythcommandlineparser.h"

class MythExternRecorderCommandLineParser : public MythCommandLineParser
{
  public:
    MythExternRecorderCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};
