
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythAVTestCommandLineParser : public MythCommandLineParser
{
  public:
    MythAVTestCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

