#ifndef VIDEOPOPUPS_H_
#define VIDEOPOPUPS_H_

#include <mythtv/libmythui/mythscreentype.h>

#include "videoutils.h"

class Metadata;

class MythUIButtonList;
class MythUIButtonListItem;

class CastDialog : public MythScreenType
{
    Q_OBJECT

  public:
    CastDialog(MythScreenStack *lparent, Metadata *metadata);

    bool Create();

  private:
    Metadata *m_metadata;
};

class PlotDialog : public MythScreenType
{
    Q_OBJECT

  public:
    PlotDialog(MythScreenStack *lparent, Metadata *metadata);

    bool Create();

  private:
    Metadata *m_metadata;
};

class SearchResultsDialog : public MythScreenType
{
    Q_OBJECT

  public:
    SearchResultsDialog(MythScreenStack *lparent,
            const SearchListResults &results);

    bool Create();

 signals:
    void haveResult(QString);

  private:
    SearchListResults m_results;
    MythUIButtonList *m_resultsList;

  private slots:
    void sendResult(MythUIButtonListItem *item);
};

#endif
