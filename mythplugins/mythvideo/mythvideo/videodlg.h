#ifndef VIDEODIALOG_H_
#define VIDEODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

#include "metadata.h"

class VideoFilterSettings;


class VideoDialog : public MythDialog
{
    Q_OBJECT
    
    public:
        enum DialogType { DLG_BROWSER, DLG_GALLERY, DLG_TREE };
        
        VideoDialog(DialogType _myType, MythMainWindow *_parent, 
                    const char* _winName, const char *_name = 0);
        
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
        virtual void slotParentalLevelChanged() {cerr << "VideoDialog::parseContainer" << endl;}
        virtual void slotWatchVideo();
            
    protected:
        virtual void updateBackground(void);
        virtual void parseContainer(QDomElement&) = 0;
        virtual void loadWindow(QDomElement &element);
        virtual void handleMetaFetch(Metadata*){}
        virtual void fetchVideos();
        virtual void setParentalLevel(int which_level);
        void shiftParental(int amount);
        bool createPopup();
        void cancelPopup(void);
        void doMenu(bool info);
        QButton* addDests(MythPopupBox* _popup = NULL);
        
        QPixmap myBackground;
        int currentParentalLevel;        
        Metadata* curitem;
        MythPopupBox* popup;
        bool expectingPopup;
        QRect fullRect;
        DialogType myType;
        
        bool allowPaint;
        
        XMLParse *theme;
        QDomElement xmldata;
        
        VideoFilterSettings *currentVideoFilter;
};

#endif
