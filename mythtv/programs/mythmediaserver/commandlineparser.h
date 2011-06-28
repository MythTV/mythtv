
#include <QString>

#include "mythcommandlineparser.h"

class MythMediaServerCommandLineParser : public MythCommandLineParser
{
  public:
    MythMediaServerCommandLineParser(); 
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

