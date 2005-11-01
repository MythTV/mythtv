#ifndef CUSTOMRECORD_H_
#define CUSTOMRECORD_H_

#include <qdatetime.h>
#include <qhbox.h>
#include "libmyth/mythwidgets.h"
#include "NuppelVideoPlayer.h"

#include <pthread.h>

class QLabel;
class ProgramInfo;

class CustomRecord : public MythDialog
{
    Q_OBJECT
  public:

    CustomRecord(MythMainWindow *parent, const char *name = 0);
   ~CustomRecord(void);
   
  signals:
    void dismissWindow();

  protected slots:
    void ruleChanged(void);
    void textChanged(void);
    void clauseChanged(void);
    void addClicked(void);
    void testClicked(void);
    void recordClicked(void);
    void cancelClicked(void);

  private:
    bool checkSyntax(void);

    int prevItem;

    QStringList m_recid;
    QStringList m_recsub;
    QStringList m_recdesc;

    QStringList m_cfrom;
    QStringList m_csql;

    MythComboBox *m_rule;
    MythRemoteLineEdit *m_title;
    MythComboBox *m_clause;
    MythRemoteLineEdit *m_preview;
    MythRemoteLineEdit *m_subtitle;
    MythRemoteLineEdit *m_description;
    MythPushButton *m_addButton;
    MythPushButton *m_testButton;
    MythPushButton *m_recordButton;
    MythPushButton *m_cancelButton;
};

#endif
