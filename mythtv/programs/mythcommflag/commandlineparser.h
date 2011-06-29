
#include <QString>

#include "mythcommandlineparser.h"

class MythCommFlagCommandLineParser : public MythCommandLineParser
{
  public:
    MythCommFlagCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

