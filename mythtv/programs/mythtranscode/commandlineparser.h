
#include <QString>

#include "mythcommandlineparser.h"

class MythTranscodeCommandLineParser : public MythCommandLineParser
{
  public:
    MythTranscodeCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

