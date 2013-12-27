#ifndef LOOKUP_H_
#define LOOKUP_H_

#include <QObject>
#include <QList>

#include "programinfo.h"
#include "metadatafactory.h"

class LookerUpper : public QObject
{
  public:
    LookerUpper();
    ~LookerUpper();

    bool AllOK() { return m_metadataFactory->VideoGrabbersFunctional(); };

    bool StillWorking();

    void HandleSingleRecording(const uint chanid,
                               const QDateTime &starttime,
                               bool updaterules = false);
    void HandleAllRecordings(bool updaterules = false);
    void HandleAllRecordingRules(void);
    void HandleAllArtwork(bool aggressive = false);

    void CopyRuleInetrefsToRecordings();

  private:
    void customEvent(QEvent *event);

    MetadataFactory      *m_metadataFactory;

    QList<ProgramInfo*>   m_busyRecList;
    bool                  m_updaterules;
    bool                  m_updateartwork;
};

#endif //LOOKUP_H_
