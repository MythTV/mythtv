#ifndef VIDEOFILTER_H_
#define VIDEOFILTER_H_

/*
    videofilter.h

    (c) 2003 Xavier Hervy
    Part of the mythTV project
    
    Class to let user filter the video list

*/

#include <iostream>
using namespace std;
#include <mythtv/mythdialogs.h>

#include "metadata.h"
class VideoFilterSettings
{
    public :
        VideoFilterSettings(bool loaddefaultsettings = true,  
                            const QString& _prefix = "");
        VideoFilterSettings(VideoFilterSettings *other);
        ~VideoFilterSettings();
        QString BuildClauseFrom();
        QString BuildClauseWhere();
        QString BuildClauseOrderBy();
        void saveAsDefault();

        enum ordering {
            // These values must be explicitly assigned; they represent
            // database values
            kOrderByTitle = 0,
            kOrderByYearDescending = 1,
            kOrderByUserRatingDescending = 2,
            kOrderByLength = 3,
        };
        
        int getCategory(void){return category;};
        void setCategory (int lcategory){category = lcategory;};
        int getGenre(void){return genre;};
        void setGenre (int lgenre){genre = lgenre;};
        int getCountry(void){return country;};
        void setCountry(int lcountry){country = lcountry;};
        int getYear(void){return year;};
        void setYear (int lyear){year = lyear;};
        int getRuntime(void){return runtime;};
        void setRuntime (int lruntime){runtime = lruntime;};
        int getUserrating(void){return userrating;};
        void setUserrating (int luserrating){userrating = luserrating;};
        int getBrowse(void){return browse;};
        void setBrowse(int lbrowse){browse = lbrowse;};
        enum ordering getOrderby (void) {return orderby;};
        void setOrderby (enum ordering lorderby) {orderby = lorderby;};
    
    private : 
        int category;
        int genre;
        int country;
        int year;
        int runtime;
        int userrating;
        int browse;
        enum ordering orderby;
        QString prefix;
};

class VideoFilterDialog : public MythThemedDialog
{

  Q_OBJECT
  
    //
    //  Dialog to manipulate the data
    //
    
  public:
  
    VideoFilterDialog(VideoFilterSettings *settings,
                       MythMainWindow *parent, 
                       QString window_name,
                       QString theme_filename,
                       const char* name = 0);
    ~VideoFilterDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();
//    QString BuildClauseFrom();
//    QString BuildClauseWhere();

  public slots:
  
    void takeFocusAwayFromEditor(bool up_or_down);
    void saveAndExit();
    void saveAsDefault();
    void setYear(int new_year);
    void setUserRating(int new_userrating);
    void setCategory(int new_category);
    void setCountry(int new_country);
    void setGenre(int new_genre);
    void setRunTime(int new_runtime);
    void setBrowse(int new_browse);
    void setOrderby(/* enum VideoFilterSettings::ordering */ int new_orderby);
 private:
    void update_numvideo();
    VideoFilterSettings *originalSettings;
    VideoFilterSettings *currentSettings;
    //  
    //  GUI Stuff
    //
//    UISelectorType      *showlevel_select;
    UISelectorType      *browse_select;
    UISelectorType      *orderby_select;
    UISelectorType      *year_select;
    UISelectorType  *userrating_select;
    UISelectorType  *category_select;
    UISelectorType  *country_select;
    UISelectorType  *genre_select;
    UISelectorType  *runtime_select;
    UITextButtonType    *save_button;
    UITextButtonType    *done_button;
    UITextType*numvideos_text; 
};


#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
