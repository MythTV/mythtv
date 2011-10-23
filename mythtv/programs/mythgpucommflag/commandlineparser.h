
#include <QString>

#include "mythcommandlineparser.h"

class MythGPUCommFlagCommandLineParser : public MythCommandLineParser
{
  public:
    MythGPUCommFlagCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

