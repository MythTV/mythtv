// -*- Mode: c++ -*-

#ifndef _MYTH_LOGSERVER_COMMAND_LINE_PARSER_H_
#define _MYTH_LOGSERVER_COMMAND_LINE_PARSER_H_

#include "mythcommandlineparser.h"

class MythLogServerCommandLineParser : public MythCommandLineParser
{
  public:
    MythLogServerCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

#endif // _MYTH_LOGSERVER_COMMAND_LINE_PARSER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
