
#include <QString>

#include "mythcommandlineparser.h"

class MythMessageCommandLineParser : public MythCommandLineParser
{
  public:
    MythMessageCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

