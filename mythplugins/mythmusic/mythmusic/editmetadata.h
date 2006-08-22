#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>
using namespace std;

#include <mythtv/mythdialogs.h>

class Metadata;

class UIPhoneEntry;

class EditMetadataDialog : public MythThemedDialog
{

  Q_OBJECT

  public:

    EditMetadataDialog(Metadata *source_metadata,
                       MythMainWindow *parent,
                       QString window_name,
                       QString theme_filename,
                       const char* name = 0);
    ~EditMetadataDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();

  public slots:

    void closeDialog();
    void searchArtist();
    void searchCompilationArtist();
    void searchAlbum();
    void searchGenre();
    void incRating(bool up_or_down);
    void showSaveMenu();
    void saveToDatabase();
    void saveToFile();
    void saveAll();
    void cancelPopup();
    void editLostFocus();
    void checkClicked(bool state);
  private:

    bool showList(QString caption, QString &value);
    
    Metadata *m_metadata, *m_sourceMetadata ;
    MythPopupBox *popup;
    //
    //  GUI stuff
    //

    UIRemoteEditType    *artist_edit;
    UIRemoteEditType    *compilation_artist_edit;
    UIRemoteEditType    *album_edit;
    UIRemoteEditType    *title_edit;
    UIRemoteEditType    *genre_edit;
    UIRemoteEditType    *year_edit;
    UIRemoteEditType    *track_edit;
    
    UITextType          *lastplay_text;
    UITextType          *playcount_text;
    UITextType          *filename_text;

    UIRepeatedImageType *rating_image;

    UIPushButtonType    *searchartist_button;
    UIPushButtonType    *searchcompilation_artist_button;
    UIPushButtonType    *searchalbum_button;
    UIPushButtonType    *searchgenre_button;
    UIPushButtonType    *rating_button;
    
    UICheckBoxType      *compilation_check;
    
    UITextButtonType    *done_button;
    UITextButtonType    *dbStatistics_button;
    
    QStringList         searchList;
};

#endif
