#ifndef CUSTOMPRIORITY_H_
#define CUSTOMPRIORITY_H_

#include <qdatetime.h>
#include <qhbox.h>
#include "libmyth/mythwidgets.h"
#include "NuppelVideoPlayer.h"

#include <pthread.h>

class QLabel;
class ProgramInfo;

class MPUBLIC CustomPriority : public MythDialog
{
    Q_OBJECT
  public:

    CustomPriority(MythMainWindow *parent, const char *name = 0,
                 ProgramInfo *m_pginfo = NULL);
   ~CustomPriority(void);
   
  signals:
    void dismissWindow();

  protected slots:
    void ruleChanged(void);
    void textChanged(void);
    void clauseChanged(void);
    void addClicked(void);
    void testClicked(void);
    void installClicked(void);
    void deleteClicked(void);
    void cancelClicked(void);
    void testSchedule(void);

  private:
    bool checkSyntax(void);

    int prevItem;

    QString addString;

    QStringList m_recpri;
    QStringList m_recdesc;

    QStringList m_csql;

    MythComboBox *m_rule;
    MythRemoteLineEdit *m_title;
    MythComboBox *m_clause;
    MythSpinBox *m_value;
    MythRemoteLineEdit *m_description;
    MythPushButton *m_addButton;
    MythPushButton *m_testButton;
    MythPushButton *m_installButton;
    MythPushButton *m_deleteButton;
    MythPushButton *m_cancelButton;
};

#endif
