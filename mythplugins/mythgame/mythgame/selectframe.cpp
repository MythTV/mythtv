#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qbitmap.h>
#include <iostream>
#include <qlabel.h>
#include <qimage.h>
#include <stdlib.h>
#include <qframe.h>

using namespace std;

#include "rominfo.h"
#include "selectframe.h"
#include "gamehandler.h"

#include <mythtv/mythcontext.h>

void SelectFrame::keyPressEvent( QKeyEvent *e )
{
  if(e)
  {
      switch(e->key())
      {
      case Key_Tab:
      case Key_Escape: 
          clearFocus();
          break;
      case Key_Enter:
      case Key_Space:
          CallSelection();
          break;
      case Key_Up:
          UpEvent();
          break;
      case Key_Down:
          DownEvent();
          break;
      case Key_Left:
          LeftEvent();
          break;
      case Key_Right:
          RightEvent();
          break;
      case Key_E:
          EditEvent();
          break;
      default:
          QFrame::keyPressEvent(e);
          break;
      }
  }
  else
      QFrame::keyPressEvent(e);
}

void SelectFrame::focusInEvent(QFocusEvent* e)
{
    e = e;
    if(mButtons[mCurrentRow][mCurrentColumn]->isVisible())
    {
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::Box | QFrame::Plain);
        mButtons[mCurrentRow][mCurrentColumn]->setLineWidth((int)(3 * wmult));
        emit gameChanged(RomList->current()->Gamename());
    }
    if((int)RomList->count() > mRows * mColumns)
    {
        mScrollBar->show();
    }    
    update();
}

void SelectFrame::EditEvent()
{
    GameHandler::EditSettings(this, RomList->current());
}

void SelectFrame::focusOutEvent(QFocusEvent* e)
{
    e = e;
    if(mButtons[mCurrentRow][mCurrentColumn]->isVisible())
    {
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::NoFrame);
    }
    mScrollBar->hide();
    emit gameChanged("");
    update();
}

void SelectFrame::CallSelection()
{
  GameHandler::Launchgame(RomList->current());
  raise();
  setActiveWindow();
}

void SelectFrame::UpEvent()
{
    int current = RomList->at();
    current-= mColumns;
    if(current >= 0)
    {
        if(mCurrentRow > 0)
        {
            mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::NoFrame);
            mCurrentRow--;
            mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::Box | QFrame::Plain);
            mButtons[mCurrentRow][mCurrentColumn]->setLineWidth((int)(3 * wmult));
        }
        else
        {
            setButtons(RomList->at(current - current % mColumns));
        }
        RomList->at(current);
        mScrollBar->setValue(mScrollBar->value() +- 1);
        emit gameChanged(RomList->current()->Gamename());
        update();
    }
}

void SelectFrame::DownEvent()
{
    int current = RomList->at();
    if((int)RomList->count() > current + mColumns - current % mColumns)
    {
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::NoFrame);
        if(mCurrentRow < mRows - 1)
        {
            mCurrentRow++;
        }
        else
        {
            setButtons(RomList->at(current - (current % mColumns + mColumns * (mRows - 2))));
        }
        if(current + mColumns < (int)RomList->count())
        {
            RomList->at(current + mColumns);
        }
        else
        {
            RomList->last();
            mCurrentColumn = RomList->count() % mColumns - 1;
        }
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::Box | QFrame::Plain);
        mButtons[mCurrentRow][mCurrentColumn]->setLineWidth((int)(3 * wmult));
        mScrollBar->setValue(mScrollBar->value() + 1);
        emit gameChanged(RomList->current()->Gamename());
        update();
    }
}

void SelectFrame::LeftEvent()
{
    int current = RomList->at();
    if(current % mColumns != 0)
    {
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::NoFrame);
        mCurrentColumn--;
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::Box | QFrame::Plain);
        mButtons[mCurrentRow][mCurrentColumn]->setLineWidth((int)(3 * wmult));
        RomList->prev();
        emit gameChanged(RomList->current()->Gamename());
        update();
    }
}

void SelectFrame::RightEvent()
{
    int current = RomList->at();
    if(current % mColumns != mColumns - 1 && current < (int)RomList->count() - 1)
    {
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::NoFrame);
        mCurrentColumn++;
        mButtons[mCurrentRow][mCurrentColumn]->setFrameStyle(QFrame::Box | QFrame::Plain);
        mButtons[mCurrentRow][mCurrentColumn]->setLineWidth((int)(3 * wmult));
        RomList->next();
        emit gameChanged(RomList->current()->Gamename());
        update();
    }
}

SelectFrame::SelectFrame(QWidget * parent, const char * name, WFlags f):
    QFrame(parent,name,f),
    mColumns(0),
    mMinSpacer(0),
    mHSpacer(0),
    mVSpacer(0),
    mWidth(0),
    mImageSize(0),
    mRows(0),
    mButtons(NULL),
    RomList(NULL)
{
    mScrollBar = new QScrollBar(Qt::Vertical, this); 

    int screenheight = 0, screenwidth = 0;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
}

void SelectFrame::setDimensions()
{
    const int ScrollWidth = mScrollBar->sizeHint().width(); 
    mScrollBar->setGeometry(width() - ScrollWidth,0,ScrollWidth,height());
    mScrollBar->hide();
    mColumns = gContext->GetNumSetting("ShotCount"); //will come from a setting.
    mMinSpacer = (int)(5 * wmult);
    mWidth = maximumWidth() - ScrollWidth;
    mImageSize = (mWidth - ((mColumns - 1) * mMinSpacer) - (mMinSpacer * 2)) / mColumns;
    if(mImageSize > height())
        mImageSize = height() - (mMinSpacer * 2);
    mRows = 0;
    for(int i = height();i >= mImageSize;i-=(mImageSize + mMinSpacer))
        mRows++;
    mHSpacer = (mWidth - (mColumns * mImageSize)) / (mColumns + 1);
    mVSpacer = (height() - (mRows * mImageSize)) / (mRows + 1);
    mButtons = new QLabel**[mRows];
    for(int i = 0;i < mRows;i++)
    {
        mButtons[i] = new QLabel*[mColumns];
        for(int j = 0;j < mColumns;j++)
        {
            mButtons[i][j] = new QLabel(this);
        }
    }
}

void SelectFrame::setRomlist(QPtrList<RomInfo> *romlist)
{
    if(!romlist)
      return;
    if(RomList)
    {
        RomList->setAutoDelete(true);
        RomList->clear();
        delete RomList;
    }
    RomList = romlist;
    setButtons(RomList->first());
    if(mButtons[0][0]->isVisible())
    {
        mCurrentRow = 0;
        mCurrentColumn = 0;
    }
    int Max = RomList->count() / mColumns - 1;
    if(RomList->count() % mColumns)
        Max++;
    mScrollBar->setRange(0, Max);
    mScrollBar->setValue(0);
    RomList->first();
}

void SelectFrame::setButtons(RomInfo *first)
{
    int x = mHSpacer, y = mVSpacer;
    QImage ButtonImage;
    QPixmap ButtonMap;
    QString ImageName;
    RomInfo *pCurrent = first;

    for(int i = 0;i < mRows;i++)
    {
        for(int j = 0;j < mColumns;j++)
        {
            if(pCurrent)
            {
                mButtons[i][j]->setGeometry(x,y,mImageSize,mImageSize);
                if(pCurrent->FindImage("screenshot",&ImageName))
                {
                    ButtonImage.load(ImageName);
                    if(ButtonImage.height() > ButtonImage.width())
                        ButtonImage = ButtonImage.scaleHeight(mImageSize);
                    else
                        ButtonImage = ButtonImage.scaleWidth(mImageSize);
                    ButtonMap.convertFromImage(ButtonImage);
                    mButtons[i][j]->setPixmap(ButtonMap);
                    mButtons[i][j]->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                }
                else
                {
                    mButtons[i][j]->setText(pCurrent->Gamename());
                    mButtons[i][j]->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter | Qt::WordBreak);
                }
                mButtons[i][j]->show();
                pCurrent = RomList->next();
            }
            else
                mButtons[i][j]->hide();
            x+= (mImageSize + mHSpacer);
        }
        x = mHSpacer;
        y+= (mImageSize + mVSpacer);
    }
}

void SelectFrame::clearButtons()
{
    if(RomList)
    {
        RomList->setAutoDelete(true);
        RomList->clear();
        delete RomList;
    }
    RomList = NULL;

    for(int i = 0;i < mRows;i++)
    {
        for(int j = 0;j < mColumns;j++)
        {
            mButtons[i][j]->hide();
        }
    }
}    
