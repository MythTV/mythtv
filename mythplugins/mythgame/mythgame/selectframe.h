#ifndef SELCTFRAME_H_
#define SELCTFRAME_H_

#include <qwidget.h>
#include <qstringlist.h>
#include <qframe.h>
#include <qscrollbar.h>
#include <qlabel.h>

#include "rominfo.h"

class QSqlDatabase;
class QLabel;

class SelectFrame : public QFrame
{
    Q_OBJECT
  public:
    SelectFrame(QWidget * parent = 0, const char * name = 0, WFlags f = 0);
   ~SelectFrame();

    void setDimensions();
    void setRomlist(QPtrList<RomInfo> *romlist);
    void setButtons(RomInfo* first);
    void clearButtons();

  signals:
    void gameChanged(const QString &);

  protected:
    int mTotalGames;
    int mColumns;
    int mMinSpacer;
    int mHSpacer;
    int mVSpacer;
    int mWidth;
    int mImageSize;
    int mRows;
    int mCurrentColumn;
    int mCurrentRow;
    QLabel ***mButtons;
    QPtrList<RomInfo> *RomList;
    QScrollBar * mScrollBar;
    void keyPressEvent( QKeyEvent *e );
    void CallSelection();
    void UpEvent();
    void DownEvent();
    void LeftEvent();
    void RightEvent();
    void EditEvent();
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);

  private:
    float wmult, hmult;
};
  
#endif
