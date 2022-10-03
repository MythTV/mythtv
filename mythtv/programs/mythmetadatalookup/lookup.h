#ifndef LOOKUP_H_
#define LOOKUP_H_

#include <QObject>
#include <QList>

#include "libmythbase/programinfo.h"
#include "libmythmetadata/metadatafactory.h"

class LookerUpper : public QObject
{
  public:
    LookerUpper();
    ~LookerUpper() override;

    static bool AllOK() { return MetadataFactory::VideoGrabbersFunctional(); };

    bool StillWorking();

    void HandleSingleRecording(uint chanid,
                               const QDateTime &starttime,
                               bool updaterules = false);
    void HandleAllRecordings(bool updaterules = false);
    void HandleAllRecordingRules(void);
    void HandleAllArtwork(bool aggressive = false);

    static void CopyRuleInetrefsToRecordings();

  private:
    void customEvent(QEvent *event) override; // QObject

    MetadataFactory      *m_metadataFactory { nullptr };

    QList<ProgramInfo*>   m_busyRecList;
    bool                  m_updaterules     { false };
    bool                  m_updateartwork   { false };
};

#endif //LOOKUP_H_
