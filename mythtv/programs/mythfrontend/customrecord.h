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
    void textChanged(void);
    void clauseChanged(void);
    void addClicked(void);
    void testClicked(void);
    void recordClicked(void);
    void cancelClicked(void);

  private:
    bool checkSyntax(void);

    MythRemoteLineEdit *m_title;
    MythRemoteLineEdit *m_preview;
    MythComboBox *m_clause;
    QStringList m_csql;
    MythRemoteLineEdit *m_description;
    MythPushButton *m_addButton;
    MythPushButton *m_testButton;
    MythPushButton *m_recordButton;
    MythPushButton *m_cancelButton;
};

#endif
