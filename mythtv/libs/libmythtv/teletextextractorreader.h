// -*- Mode: c++ -*-

#ifndef TELETEXTEXTRACTORREADER_H
#define TELETEXTEXTRACTORREADER_H

#include <QString>
#include <QMutex>
#include <QPair>
#include <QSet>

#include "mythtvexp.h"
#include "teletextreader.h"

QString decode_teletext(int codePage, const uint8_t data[40]);

class MTV_PUBLIC TeletextExtractorReader : public TeletextReader 
{
  public:
    QSet<QPair<int, int> > GetUpdatedPages(void) const
    {
        return m_updated_pages;
    }

    void ClearUpdatedPages(void)
    {
        m_updated_pages.clear();
    }

  protected:
    virtual void PageUpdated(int page, int subpage);
    virtual void HeaderUpdated(
        int page, int subpage, uint8_t *page_ptr, int lang);

  private:
    QSet<QPair<int, int> > m_updated_pages;
};

#endif // TELETEXTEXTRACTORREADER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
