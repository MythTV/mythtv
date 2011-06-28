
#include <QString>

#include "mythcommandlineparser.h"

class MythFrontendCommandLineParser : public MythCommandLineParser
{
  public:
    MythFrontendCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

