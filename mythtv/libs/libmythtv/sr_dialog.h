#ifndef REC_OPT_DIALOG
#define REC_OPT_DIALOG
#include "mythdialogs.h"
#include "managedlist.h"
#include "xmlparse.h"

class RootSRGroup;
class ScheduledRecording;
class ProgramInfo;

class RecOptDialog : public MythDialog
{
    Q_OBJECT

    public:
        RecOptDialog(ScheduledRecording*, MythMainWindow *parent, const char *name = 0);
        ~RecOptDialog();


    protected:
        void paintEvent(QPaintEvent *);
        void keyPressEvent(QKeyEvent *e);
    
    protected:
        QPixmap myBackground;
        const ProgramInfo *program;
        ScheduledRecording* schedRec;

        XMLParse* theme;
        QDomElement xmldata;

        QRect infoRect;
        QRect fullRect;
        QRect listRect;
        
        int listsize;

        bool allowEvents;
        bool allowUpdates;
        bool updateAll;
        bool refillAll;
    
        ManagedList listMenu;
        QGuardedPtr<RootSRGroup> rootGroup;
        void updateInfo(QPainter *p);
        void updateBackground(void);
        void LoadWindow(QDomElement &);
        void constructList(void);
        QMap<QString, QString> infoMap;
};

#endif
