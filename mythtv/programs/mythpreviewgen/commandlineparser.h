
#include <QString>

#include "mythcommandlineparser.h"

class MythPreviewGeneratorCommandLineParser : public MythCommandLineParser
{
  public:
    MythPreviewGeneratorCommandLineParser();
    void LoadArguments(void);
  protected:
//    QString GetHelpHeader(void) const;
};


