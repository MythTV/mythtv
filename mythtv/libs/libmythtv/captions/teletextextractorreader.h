// -*- Mode: c++ -*-

#ifndef TELETEXTEXTRACTORREADER_H
#define TELETEXTEXTRACTORREADER_H

#include <QString>
#include <QMutex>
#include <QPair>
#include <QSet>

#include "mythtvexp.h"
#include "captions/teletextreader.h"

QString decode_teletext(int codePage, const tt_line_array& data);

class MTV_PUBLIC TeletextExtractorReader : public TeletextReader 
{
  public:
    QSet<QPair<int, int> > GetUpdatedPages(void) const
    {
        return m_updatedPages;
    }

    void ClearUpdatedPages(void)
    {
        m_updatedPages.clear();
    }

  protected:
    void PageUpdated(int page, int subpage) override; // TeletextReader
    void HeaderUpdated(int page, int subpage, tt_line_array& page_ptr, int lang) override; // TeletextReader

  private:
    QSet<QPair<int, int> > m_updatedPages;
};

#endif // TELETEXTEXTRACTORREADER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
