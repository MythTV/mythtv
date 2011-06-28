
#include <QString>

#include "mythcommandlineparser.h"

class MythAVTestCommandLineParser : public MythCommandLineParser
{
  public:
    MythAVTestCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

