
#include <QString>

#include "mythcommandlineparser.h"

class MythPreviewGeneratorCommandLineParser : public MythCommandLineParser
{
  public:
    MythPreviewGeneratorCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const override; // MythCommandLineParser
};


