// -*- Mode: c++ -*-

#include <QString>

#include "mythcommandlineparser.h"

class MythBackendCommandLineParser : public MythCommandLineParser
{
  public:
    MythBackendCommandLineParser(); 
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};
