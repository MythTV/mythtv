/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _LOG_LIST_H_
#define _LOG_LIST_H_

#include "settings.h"

class LogList : public ListBoxSetting, public TransientStorage
{
  public:
    LogList();

    void AppendLine(const QString &text);

  private:
    uint idx;
};

#endif // _LOG_LIST_H_
