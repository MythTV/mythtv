
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythFillDatabaseCommandLineParser : public MythCommandLineParser
{
  public:
    MythFillDatabaseCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
//    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

