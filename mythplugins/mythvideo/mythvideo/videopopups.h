#ifndef VIDEOPOPUPS_H_
#define VIDEOPOPUPS_H_

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuitextedit.h>

// MythVideo headers
#include "parentalcontrols.h"
#include "videoutils.h"
#include "metadata.h"

class Metadata;
class VideoList;
class ParentalLevel;

class CastDialog : public MythScreenType
{
    Q_OBJECT

  public:
    CastDialog(MythScreenStack *parent, Metadata *metadata);

    bool Create(void);

  private:
    Metadata *m_metadata;
};

class PlotDialog : public MythScreenType
{
    Q_OBJECT

  public:
    PlotDialog(MythScreenStack *parent, Metadata *metadata);

    bool Create(void);

  private:
    Metadata *m_metadata;
};

class SearchResultsDialog : public MythScreenType
{
    Q_OBJECT

  public:
    SearchResultsDialog(MythScreenStack *parent,
                                            const SearchListResults &results);

    bool Create(void);

 signals:
    void haveResult(QString);

  private:
    SearchListResults m_results;
    MythUIButtonList *m_resultsList;

  private slots:
    void sendResult(MythUIButtonListItem *item);
};

#endif
