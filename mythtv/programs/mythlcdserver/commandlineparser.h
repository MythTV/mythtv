
#include <QString>

#include "mythcommandlineparser.h"

class MythLCDServerCommandLineParser : public MythCommandLineParser
{
  public:
    MythLCDServerCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

