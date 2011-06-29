
#include <QString>

#include "mythcommandlineparser.h"

class MythFillDatabaseCommandLineParser : public MythCommandLineParser
{
  public:
    MythFillDatabaseCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};

