
#include <QString>

#include "mythcommandlineparser.h"

class MythWelcomeCommandLineParser : public MythCommandLineParser
{
  public:
    MythWelcomeCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

