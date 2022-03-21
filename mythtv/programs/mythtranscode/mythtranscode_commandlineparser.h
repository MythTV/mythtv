
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythTranscodeCommandLineParser : public MythCommandLineParser
{
  public:
    MythTranscodeCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const;
};

