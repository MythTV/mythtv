
#include <QString>

#include "mythcommandlineparser.h"

class MythShutdownCommandLineParser : public MythCommandLineParser
{
  public:
    MythShutdownCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

