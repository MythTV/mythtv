#ifndef VIDEODIALOG_H_
#define VIDEODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qsqldatabase.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

#include "metadata.h"

class VideoFilterSettings;


class VideoDialog : public MythThemedDialog
{
    Q_OBJECT
    
    public:
        enum DialogType { DLG_BROWSER, DLG_GALLERY, DLG_TREE };
        
        VideoDialog(DialogType _myType, QSqlDatabase *_db, 
                    MythMainWindow *_parent, const char* _winName, const char *_name = 0);
        
        virtual ~VideoDialog();
        
        virtual void playVideo(Metadata *someItem);
        
        QString getHandler(Metadata *someItem);
        QString getCommand(Metadata *someItem);
        bool checkParentPassword();
        
    protected slots:
        void slotDoCancel();
        void slotVideoTree();
        void slotVideoGallery();
        void slotVideoBrowser();
        void slotViewPlot();
        void slotDoFilter();
        void exitWin();
        virtual void slotParentalLevelChanged() {}
        virtual void slotWatchVideo();
            
    protected:
        virtual void handleMetaFetch(Metadata*){}
        virtual void fetchVideos();
        virtual void setParentalLevel(int which_level);
        void shiftParental(int amount);
        bool createPopup();
        void cancelPopup(void);
        QButton* addDests(MythPopupBox* _popup = NULL);
        
        int currentParentalLevel;        
        QSqlDatabase *db;
        Metadata* curitem;
        MythPopupBox* popup;
        bool expectingPopup;
        QRect fullRect;
        DialogType myType;
        
        bool allowPaint;
        
        VideoFilterSettings *currentVideoFilter;
};

#endif
