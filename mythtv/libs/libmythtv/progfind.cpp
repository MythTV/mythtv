/*
        MythProgramFind
	January 19th, 2003

        By John Danner
*/

#include <qlabel.h>
#include <qaccel.h>
#include <fstream>
#include <iostream>
#include <qlayout.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <qimage.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qheader.h>
#include <qpixmap.h>
#include <unistd.h>

#include "progfind.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "tv.h"

using namespace std;

#include "libmyth/mythcontext.h"


void RunProgramFind(MythContext *context, bool thread,
                        TV *player)
{
    if (thread)
        qApp->lock();

    ProgFinder programFind(context, player);

    if (thread)
    {
        programFind.show();
        qApp->unlock();

        while (programFind.isVisible())
            usleep(50);
    }
    else
        programFind.exec();

    return;
}


ProgFinder::ProgFinder(MythContext *context, TV *player,
                         QWidget *parent, const char *name)
           : MythDialog(context, parent, name)
{
    m_context = context;
    m_player = player;
    m_db = QSqlDatabase::database();

    context->KickDatabase(m_db);

    baseDir = context->GetInstallPrefix();

    accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Return), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(Key_I), this, SLOT(getInfo()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(escape()));
    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    topFrame = new QFrame(this);
    topFrame->setPaletteForegroundColor(prog_fgColor);
    topFrameLayout = new QHBoxLayout(topFrame, 0);

    curSearch = 10;
    progData = new QString[1];
    listCount = 1;
    showsPerListing = m_context->GetNumSetting("showsPerListing");;
    if (showsPerListing == 0 || (showsPerListing % 2) == 0)
	showsPerListing = 9;

    inSearch = 0;
    pastInitial = false;
    searchCount = 37;

    initData = new QString[(int)(searchCount*showsPerListing)];
    gotInitData = new int[searchCount];
    searchData = new QString[searchCount];
    int curLabel = 0;

    for (int charNum = 48; charNum < 91; charNum++)
    {
        if (charNum == 58)
        	charNum = 65;

	if (curLabel == 30)
	{
		gotInitData[curLabel] = 0;
		searchData[curLabel] = "The";
    		curLabel++;
		
	}

	gotInitData[curLabel] = 0; 
        searchData[curLabel] = (char)charNum;
	curLabel++;
    }

    update_Timer = new QTimer(this);
    connect(update_Timer, SIGNAL(timeout()), SLOT(update_timeout()) );

    setupColorScheme();
    setupLayout();

    getSearchData(curSearch);
    showSearchList();

    update_Timer->start((int)(100));

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);

    showInfo = false;

    showFullScreen();
    setActiveWindow();
    raise();
    setFocus();

}

ProgFinder::~ProgFinder()
{
    if (inSearch > 0)
        delete [] progData;
    if (inSearch == 2)
    	delete [] showData;
    delete [] searchData;
    delete [] initData; 
    delete [] abcList;
    delete [] progList;
    delete [] showList;
    delete programTitle;
    delete subTitle;
    delete description;
    delete callChan;
    delete recordingInfo;

    delete [] gotInitData;

    delete update_Timer;

    delete topDataLayout;
    delete topFrameLayout;
    delete topFrame;
    delete programFrame;
    delete main;

    delete accel;
}

void ProgFinder::escape()
{
    unsetCursor();
    emit killTheApp();
}


void ProgFinder::getInfo()
{
    int rectype = 0;
    QString data = "";
    if (inSearch == 2)
    {
	showInfo = 1;
	ProgramInfo *curPick = ProgramInfo::GetProgramAtDateTime(curChannel,curDateTime + "50");

  	if (curPick)
    	{
        	InfoDialog diag(m_context, curPick, this, "Program Info");
        	diag.setCaption("BLAH!!!");
        	diag.exec();
    	}
    	else
	{
        	return;
	}
	showInfo = 0;

    	curPick->GetProgramRecordingStatus(m_db);

	getRecordingInfo();

	for (int i = 0; i < showCount; i++)
	{
		rectype = checkRecordingStatus(i);
                showData[i].recording = rectype;

                switch (rectype)
                {
                        case ScheduledRecording::SingleRecord:
                            data = "Recording just this showing";
                            break;
                        case ScheduledRecording::TimeslotRecord:
                            data = "Recording when shown in this timeslot";
                            break;
                        case ScheduledRecording::ChannelRecord:
                            data = "Recording when shown on this channel";
                            break;
                        case ScheduledRecording::AllRecord:
                            data = "Recording all showings";
                            break;
                        case ScheduledRecording::NotRecording:
                            data = "Not recording this showing";
                            break;
                        default:
                            data = "Error!";
                }

                showData[i].recText = data;
	}

	showShowingList();

    	setActiveWindow();
    	setFocus();

    }

}

void ProgFinder::update_timeout()
{
	if (pastInitial == false)
	{
		update_Timer->stop();
		pastInitial = true;
		getInitialProgramData();
	}
	else
	{
	  if (inSearch == 0 && gotInitData[curSearch] == 0)
	  {
	    int cnt = 0;

    	    for (int j = 0; j < searchCount; j++)
   	    {
        	  if (gotInitData[j] > 1)
        	        cnt++;
 	    } 

 	    int amountDone = (int)(100.0 * (float)((float)cnt / (float)searchCount));

 	    QString data = QString(" Loading Data...%1% Complete").arg(amountDone);

	    (&progList[(int)(showsPerListing / 2)])->setText(data);
   	  }
	}
}

void ProgFinder::setupColorScheme()
{

    Settings *themed = m_context->qtconfig();
    QString curColor = "";
    curColor = themed->GetSetting("time_bgColor");
    if (curColor != "")
        time_bgColor = QColor(curColor);

    curColor = themed->GetSetting("time_fgColor");
    if (curColor != "")
        time_fgColor = QColor(curColor);

    curColor = themed->GetSetting("curProg_bgColor");
    if (curColor != "")
        curProg_bgColor = QColor(curColor);

    curColor = themed->GetSetting("curProg_fgColor");
    if (curColor != "")
        curProg_fgColor = QColor(curColor);

    curColor = themed->GetSetting("prog_bgColor");
    if (curColor != "")
        prog_bgColor = QColor(curColor);

    curColor = themed->GetSetting("prog_fgColor");
    if (curColor != "")
        prog_fgColor = QColor(curColor);

    curColor = themed->GetSetting("chan_bgColor");
    if (curColor != "")
        abc_bgColor = QColor(curColor);
	
    curColor = themed->GetSetting("chan_fgColor");
    if (curColor != "")
        abc_fgColor = QColor(curColor);

    curColor = themed->GetSetting("curRecProg_bgColor");
    if (curColor != "")
        recording_bgColor = QColor(curColor);
}

void ProgFinder::setupLayout()
{
	 QFont callFont("Arial", (int)(24 * hmult), QFont::Bold);

	QFont titleFont("Arial", (int)(22 * hmult), QFont::Bold);
	QFontMetrics titleMetric(titleFont);

	QFont subtFont("Arial", (int)(20 * hmult), QFont::Bold);
	QFontMetrics subtMetric(subtFont);

	QFont descFont("Arial", (int)(18 * hmult), QFont::Bold);
	QFontMetrics descMetric(descFont);

	QFont listFont("Arial", (int)(18 * hmult), QFont::Bold);
    	QFontMetrics fontMet(listFont);

	programFrame = new QFrame(this);
	QFrame *topDataFrame = new QFrame(this);
	programFrame->setBackgroundOrigin(WindowOrigin);

	main = new QVBoxLayout(this, (int)(30 * hmult), (int)(15 * hmult));
	topDataLayout = new QHBoxLayout(topDataFrame, 0, 0);
	topDataFrame->setPaletteBackgroundColor(prog_bgColor);

	topDataFrame->setMaximumWidth((int)(735*wmult));
  	topDataFrame->setMinimumWidth((int)(735*wmult));
	topDataFrame->setFrameStyle( QFrame::Panel | QFrame::Raised );

	main->addWidget(topDataFrame, 0, 0);
	main->addWidget(programFrame, 0, 0);

	topDataLayout->addStrut((int)(125*hmult));

	QVBoxLayout *leftSide = new QVBoxLayout(0, 0, 0);
	
	programTitle = new QLabel("Select a letter...", topDataFrame);
	programTitle->setFont(titleFont);
	programTitle->setMaximumWidth((int)(500*wmult));
	programTitle->setMaximumHeight(titleMetric.height());
	programTitle->setPaletteForegroundColor(prog_fgColor);
	programTitle->setPaletteBackgroundColor(prog_bgColor);

    	subTitle = new QLabel("", topDataFrame);
	subTitle->setFont(subtFont);
	subTitle->setMaximumWidth((int)(500*wmult));
	subTitle->setMaximumHeight(subtMetric.height());
	subTitle->setPaletteForegroundColor(prog_fgColor);
	subTitle->setPaletteBackgroundColor(prog_bgColor);

    	description = new QLabel("Pick the letter in which the show starts with, then hit ENTER or the right arrow.", topDataFrame);
	description->setFont(descFont);
	description->setMaximumWidth((int)(500*wmult));
	description->setMaximumHeight((int)(140*hmult));
	description->setAlignment(Qt::WordBreak);
	description->setPaletteForegroundColor(prog_fgColor);
	description->setPaletteBackgroundColor(prog_bgColor);

	recordingInfo = new QLabel("", topDataFrame);
        recordingInfo->setFont(descFont);
        recordingInfo->setMaximumWidth((int)(500*wmult));
        recordingInfo->setPaletteForegroundColor(prog_fgColor);
        recordingInfo->setPaletteBackgroundColor(prog_bgColor);
	recordingInfo->setMaximumHeight(descMetric.height() + (int)(2*hmult));
	recordingInfo->setPaletteForegroundColor(curProg_fgColor);

    	callChan = new QLabel("", topDataFrame);
	callChan->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter);
	callChan->setMaximumWidth((int)(200*wmult));
	callChan->setMinimumWidth((int)(200*wmult));
	callChan->setMaximumHeight((int)(200*hmult));
	callChan->setMinimumHeight((int)(200*hmult));
	callChan->setPaletteBackgroundColor(QColor(0, 0, 0));
	callChan->setPaletteForegroundColor(QColor(255, 255, 255));
	callChan->setFont(callFont);

	leftSide->addWidget(programTitle, 0, 0);
	leftSide->addWidget(subTitle, 0, 0);
	leftSide->addWidget(description, 0, 0);
	leftSide->addWidget(recordingInfo, 0, 0);

	topDataLayout->addLayout(leftSide, 0);
	topDataLayout->addWidget(callChan, 0, 0);

	QHBoxLayout *programListLayout  = new QHBoxLayout(programFrame, 0);
	QVBoxLayout *abcVert  = new QVBoxLayout(0, 0, 0);
	QVBoxLayout *progVert  = new QVBoxLayout(0, 0, 0);
	QVBoxLayout *showVert  = new QVBoxLayout(0, 0, 0);

	abcVert->addStrut((int)(65*wmult));
	progVert->addStrut((int)(380*wmult));
	showVert->addStrut((int)(280*wmult));

	programListLayout->setResizeMode(QLayout::Minimum);
	programListLayout->addLayout(abcVert, 0);
	programListLayout->addLayout(progVert, 0);
	programListLayout->addLayout(showVert, 0);
	programListLayout->addStrut((int)(310*hmult));

	QLabel *BlankLbl = new QLabel(" ", programFrame);
	QLabel *ProgramTitleLbl = new QLabel("Program Title", programFrame);
	QLabel *ShowTimesLbl = new QLabel("Show Times", programFrame);

	BlankLbl->setPaletteBackgroundColor(time_bgColor);
	BlankLbl->setPaletteForegroundColor(prog_fgColor);
	ProgramTitleLbl->setPaletteBackgroundColor(time_bgColor);
	ProgramTitleLbl->setPaletteForegroundColor(prog_fgColor);
	ProgramTitleLbl->setAlignment( Qt::AlignHCenter);
	ShowTimesLbl->setPaletteForegroundColor(prog_fgColor);
	ShowTimesLbl->setPaletteBackgroundColor(time_bgColor);
	ShowTimesLbl->setAlignment( Qt::AlignHCenter);

	BlankLbl->setMaximumHeight(fontMet.height() + (int)(4*hmult));
	ProgramTitleLbl->setMaximumHeight(fontMet.height() + (int)(4*hmult));
	ShowTimesLbl->setMaximumHeight(fontMet.height() + (int)(4*hmult));
	
	BlankLbl->setFrameStyle( QFrame::Panel | QFrame::Raised );
	ProgramTitleLbl->setFrameStyle( QFrame::Panel | QFrame::Raised );
	ShowTimesLbl->setFrameStyle( QFrame::Panel | QFrame::Raised );

	BlankLbl->setFont(listFont);
	ProgramTitleLbl->setFont(listFont);
	ShowTimesLbl->setFont(listFont);

	abcVert->addWidget(BlankLbl, 0, 0);
	progVert->addWidget(ProgramTitleLbl, 0, 0);
	showVert->addWidget(ShowTimesLbl, 0, 0);

	abcList = new QLabel[showsPerListing](" ", programFrame);
	progList = new QLabel[showsPerListing](" ", programFrame);
	showList = new QLabel[showsPerListing](" ", programFrame);

	for (int i = 0; i < showsPerListing; i++)
	{
		if (i == (int)(showsPerListing / 2))
		{
			(&abcList[i])->setPaletteBackgroundColor(QColor(0, 0, 0));
			(&progList[i])->setPaletteBackgroundColor(curProg_bgColor);
			(&showList[i])->setPaletteBackgroundColor(curProg_bgColor);
			(&abcList[i])->setPaletteForegroundColor(curProg_fgColor);
			(&progList[i])->setPaletteForegroundColor(curProg_fgColor);
			(&showList[i])->setPaletteForegroundColor(curProg_fgColor);
		}
		else
		{
			(&abcList[i])->setPaletteForegroundColor(abc_fgColor);
                        (&progList[i])->setPaletteForegroundColor(prog_fgColor);
                        (&showList[i])->setPaletteForegroundColor(prog_fgColor);
		        (&abcList[i])->setPaletteBackgroundColor(abc_bgColor);
                        (&progList[i])->setPaletteBackgroundColor(prog_bgColor);
                        (&showList[i])->setPaletteBackgroundColor(prog_bgColor);
		}

		(&abcList[i])->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter);
		(&abcList[i])->setMaximumWidth((int)(65*wmult));
                (&progList[i])->setMaximumWidth((int)(380*wmult));
                (&showList[i])->setMaximumWidth((int)(280*wmult));

		(&abcList[i])->setFont(listFont);
                (&progList[i])->setFont(listFont);
                (&showList[i])->setFont(listFont);

		abcVert->addWidget(&(abcList[i]), 0, 0);
		progVert->addWidget(&(progList[i]), 0, 0);
		showVert->addWidget(&(showList[i]), 0, 0);
	}

	
}

void ProgFinder::cursorLeft()
{
	inSearch--;
	if (inSearch == -1)
		inSearch = 0;
	else {
		if (inSearch == 0)
		{
			programTitle->setText("Select a letter...");
			description->setText("Pick the letter in which the show starts with, then hit ENTER or the right arrow.");
			(&abcList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(QColor(0, 0, 0));
                        (&progList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(curProg_bgColor);
			showSearchList();
		}
		if (inSearch == 1)
		{
                        (&progList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(QColor(0, 0, 0));
                        (&showList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(curProg_bgColor);
			showProgramList();
			clearShowData();
		}
	}
}

void ProgFinder::cursorRight()
{
	inSearch++;
	if (inSearch == 3)
		inSearch = 2;
	else
	{
		if (inSearch == 1)
		{
		    if (gotInitData[curSearch] == 0)
		    {
			getSearchData(curSearch);
		    }
		    if (gotInitData[curSearch] >= 10)
		    {
			programTitle->setText("Select a program...");
			description->setText("Select the title of the program you wish to find. When finished return with the left arrow key. Hitting 'Info' will allow you to setup recording options.");
			(&abcList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(curProg_bgColor);
                        (&progList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(QColor(0, 0, 0));
			selectSearchData();
		    }
		    if (gotInitData[curSearch] == 1)
		    {
			programTitle->setText("No Programs");
			description->setText("There are no available programs under this search. Please select another search.");
			(&progList[(int)(showsPerListing / 2)])->setText("      !! No Programs !!");
			inSearch = 0;
		    }
		}
		if (inSearch == 2)
		{
			if (gotInitData[curSearch] > 10)
			{
                            (&progList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(curProg_bgColor);
                            (&showList[(int)(showsPerListing / 2)])->setPaletteBackgroundColor(QColor(0, 0, 0));
			    selectShowData(progData[curProgram]);
			}
			else
			{
				inSearch = 1;
			}
		}
	}
}

void ProgFinder::pageUp()
{
   if (inSearch == 0)
   {
        curSearch = curSearch - showsPerListing;
        if (curSearch < -1)
                curSearch = searchCount + curSearch;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
        	showSearchList();
   }
   if (inSearch == 1)
   {
        curProgram = curProgram - showsPerListing;
        if (curProgram <= -1)
                curProgram = listCount + curProgram;

        showProgramList();
   }
   if (inSearch == 2)
   {
	curShow = curShow - showsPerListing;
	if (curShow <= -1)
		curShow = showCount + curShow;

	showShowingList();
   }
}

void ProgFinder::pageDown()
{
   if (inSearch == 0)
   {
        curSearch = curSearch + showsPerListing;

        if (curSearch >= searchCount)
                curSearch = curSearch - searchCount;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
        	showSearchList();
   }
   if (inSearch == 1)
   {
        curProgram = curProgram + showsPerListing;
        if (curProgram >= listCount)
                curProgram = curProgram - listCount;

        showProgramList();
   }
   if (inSearch == 2)
   {
        curShow = curShow + showsPerListing;
        if (curShow >= showCount)
                curShow = curShow - showCount;

        showShowingList();
   }
}

void ProgFinder::cursorUp()
{

   if (inSearch == 0)
   {
	curSearch--;
	if (curSearch == -1)
		curSearch = searchCount - 1;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
		showSearchList();
   }
   if (inSearch == 1)
   {
	curProgram--;
        if (curProgram == -1)
	{
              curProgram = listCount - 1;
	      while (progData[curProgram] == "**!0")
		   curProgram--;

	}

	showProgramList();
   }
   if (inSearch == 2)
   {
	curShow--;
	if (curShow == -1)
	{
		curShow = showCount - 1;
		while (showData[curShow].title == "**!0")
			curShow--;
	}

	showShowingList();
   }
}

void ProgFinder::cursorDown()
{
   if (inSearch == 0)
   {
	curSearch++;
	if (curSearch >= searchCount)
		curSearch = 0;

	if (gotInitData[curSearch] <= 1)
		clearProgramList();
        else
		showSearchList();
   }
   if (inSearch == 1)
   {
        if ((curProgram + 1) >= listCount)
		curProgram = -1;

	if (progData[curProgram + 1] != "**!0")
	{
		curProgram++;
		if (curProgram == listCount)
			curProgram = 0;

		showProgramList();
	}
	else
	{
		curProgram = 0;
		showProgramList();
	}
   }
   if (inSearch == 2)
   {
        if ((curShow + 1) >= showCount)
		curShow = -1;
	
	if (showData[curShow + 1].title != "**!0")
	{
		curShow++;
		if (curShow == showCount)
			curShow = 0;

		showShowingList();
	}
	else
	{
		curShow = 0;
		showShowingList();
	}
   }
}

void ProgFinder::showSearchList()
{
	if (programTitle->text() == "No Programs")
	{
		programTitle->setText("Select a program...");
    		description->setText("Select the title of the program you wish to find. When finished return with the left arrow key. Hitting 'Info' will allow you to setup recording options");
	}

	int curLabel = 0;
	int t = 0;
	int tempSearch;
	QString placeHold;

	tempSearch = curSearch;

	for (int i = (tempSearch - ((showsPerListing - 1) / 2));
	     i < (tempSearch + ((showsPerListing + 1) / 2)); i++)
	{
		t = i;
                if (i < 0)
                        t = i + searchCount;
                if (i >= searchCount)
                        t = i - searchCount;
		if (t < 0)
			cerr << "MythProgFind: -1 Error in showSearchList()\n";

                if (searchData[t] != NULL)
                {
			if (curLabel == (int)(showsPerListing / 2))
			{
                        	(&abcList[curLabel])->setText(" " + searchData[t] + " ");
			}
			else
				(&abcList[curLabel])->setText(" " + searchData[t] + " ");
                }
                else
                {
                        (&abcList[curLabel])->setText("");
                }
                curLabel++;

	}

	if (gotInitData[curSearch] > 1)
	{
	   if (update_Timer->isActive() == true)
		update_Timer->stop();

	   curLabel = 0; 

	   for (int i = (int)(tempSearch*showsPerListing); i < (int)((tempSearch+1)*showsPerListing); i++)
	   { 
		if (initData[i] != NULL)
		{
			if (curLabel == (int)(showsPerListing / 2))
				(&progList[curLabel])->setText(" " + initData[i] + " ");
			else
				(&progList[curLabel])->setText(" " + initData[i] + " ");	
		}
		else
		{
			(&progList[curLabel])->setText("");
		}
		curLabel++;
	   }
	}
	else
	{
		update_Timer->start((int)250);
	}
}

void ProgFinder::showProgramList()
{

    if (gotInitData[curSearch] > 1)
    {
	int curLabel = 0;
	int t = 0;
	for (int i = (curProgram - ((showsPerListing - 1) / 2)); 
   	     i < (curProgram + ((showsPerListing + 1) / 2)); i++)
	{
		t = i;
		if (i < 0)
			t = i + listCount;
		if (i >= listCount)
			t = i - listCount;
	
		if (progData[t] != NULL)
		{
		   if (progData[t] != "**!0")
		   {
			if (curLabel == (int)(showsPerListing / 2))
				(&progList[curLabel])->setText(" " + progData[t] + " ");
			else
				(&progList[curLabel])->setText(" " + progData[t] + " ");
  		   }
		   else
		   	(&progList[curLabel])->setText("");
		}
		else
		{
			(&progList[curLabel])->setText("");
		}
		curLabel++;
	}

    }
}

void ProgFinder::showShowingList()
{
    int curLabel = 0;
    QString tempData = "";

    if (showCount > 0)
    {
        int t = 0;
        for (int i = (curShow - ((showsPerListing - 1) / 2));
             i < (curShow + ((showsPerListing + 1) / 2)); i++)
        {
                t = i;
                if (i < 0)
                        t = i + showCount;
                if (i >= showCount)
                        t = i - showCount;

		if ((&showData[t]) != NULL)
		{
		   if (showData[t].title != "**!0")
		   {
			(&showList[curLabel])->setText(" " + showData[t].startDisplay);
			
			if (curLabel != (int)(showsPerListing / 2))
			{
			    if (showData[t].recording > 0)
				(&showList[curLabel])->setPaletteBackgroundColor(recording_bgColor);
			    else
				(&showList[curLabel])->setPaletteBackgroundColor(prog_bgColor);
			}

		   }
		   else
		   {
			(&showList[curLabel])->setText("");
			(&showList[curLabel])->setPaletteBackgroundColor(prog_bgColor);
		   }
		}	
		else
		{
			(&showList[curLabel])->setText("");
		}
		curLabel++;
        }
	curChannel = showData[curShow].channelID;
        curDateTime = showData[curShow].starttime;
	callChan->setText(showData[curShow].channelNum + "\n" + showData[curShow].channelCallsign);
	programTitle->setText(progData[curProgram]);
	if ((showData[curShow].subtitle).length() == 0)
		subTitle->hide();
	else
		subTitle->show();
	subTitle->setText("\"" + showData[curShow].subtitle + "\"");
	if ((showData[curShow].description).length() > 1)
		description->setText(showData[curShow].startDisplay + " - " + showData[curShow].endDisplay +
				     "\n" + showData[curShow].description);
	else
		description->setText(showData[curShow].startDisplay + " - " + showData[curShow].endDisplay + ".");
	recordingInfo->setText(showData[curShow].recText);
    }
}

void ProgFinder::selectSearchData()
{
    accel->setEnabled(false);
    QDateTime progStart = QDateTime::currentDateTime();
    QString thequery;
    QString data;

    if (searchData[curSearch] == "T")
    {
    thequery = QString("SELECT title "
                       "FROM program,channel "
                       "WHERE program.title LIKE '%1%' AND program.title NOT LIKE 'The %' "
		       "AND program.chanid = channel.chanid "
                       "AND program.starttime > %2 "
                       "GROUP BY program.title "
                       "ORDER BY program.title;")
                        .arg(searchData[curSearch]).arg(progStart.toString("yyyyMMddhhmm50"));
    }
    else
    {
    thequery = QString("SELECT title "
                       "FROM program,channel "
                       "WHERE program.title LIKE '%1%' AND program.chanid = channel.chanid "
		       "AND program.starttime > %2 "
		       "GROUP BY program.title "
                       "ORDER BY program.title;")
                        .arg(searchData[curSearch]).arg(progStart.toString("yyyyMMddhhmm50"));
    }

    QSqlQuery query = m_db->exec(thequery);
 
    if (query.numRowsAffected() == -1)
    {
        cerr << "MythProgFind: Error executing query! (selectSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    delete [] progData;

    listCount = 0;

    if (query.numRowsAffected() < showsPerListing)
    {
	progData = new QString[showsPerListing];
	for (int i = 0; i < showsPerListing; i++)
               progData[i] = "**!0";
    }
    else
    	progData = new QString[(int)query.numRowsAffected()];

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
		data = QString::fromUtf8(query.value(0).toString());

		progData[listCount] = data;

		listCount++;
	}
    }

    if (query.numRowsAffected() < showsPerListing)
	listCount = showsPerListing;

    curProgram = 0;
    accel->setEnabled(true);
    showProgramList();
}

void ProgFinder::clearProgramList()
{
  if (gotInitData[curSearch] == 0)
  {
    int cnt = 0;
    for (int i = 0; i < showsPerListing; i++)
	(&progList[i])->setText("");

    for (int j = 0; j < searchCount; j++)
    {
	if (gotInitData[j] > 0)
		cnt++;
    }

    int amountDone = (int)(100.0 * (float)((float)cnt / (float)searchCount));

    QString data = QString(" Loading Data...%1% Complete").arg(amountDone);

    (&progList[(int)(showsPerListing / 2)])->setText(data);
    showSearchList();
  }
  else
  {
    showSearchList();

    programTitle->setText("No Programs");
    description->setText("There are no available programs under this search. Please select another search.");
    for (int i = 0; i < showsPerListing; i++)
        (&progList[i])->setText("");
  }
}

void ProgFinder::clearShowData()
{
    delete [] showData;

    for (int i = 0; i < showsPerListing; i++)
    {
	if (i != (int)(showsPerListing / 2))
		(&showList[i])->setPaletteBackgroundColor(prog_bgColor);
	(&showList[i])->setText("");
    }

    programTitle->setText("Select a program...");
    subTitle->setText("");
    description->setText("Select the title of the program you wish to find. When finished return with the left arrow key. Hitting 'Info' will allow you to setup recording options");
    callChan->setText("");
    recordingInfo->setText("");

}

int ProgFinder::checkRecordingStatus(int showNum)
{

    for (int j = 0; j < recordingCount; j++)
    {
	if (showData[showNum].title == curRecordings[j].title)
	{
	    	if (showData[showNum].subtitle == curRecordings[j].subtitle &&
	    	    showData[showNum].description == curRecordings[j].description)
	    	{

			if (curRecordings[j].type == ScheduledRecording::SingleRecord)
			{
				if (showData[showNum].startdatetime == curRecordings[j].startdatetime)	
				{
					return curRecordings[j].type;
				}
			}
		}
	 	if (curRecordings[j].type == ScheduledRecording::TimeslotRecord)
		{
			if ((showData[showNum].startdatetime).time() == (curRecordings[j].startdatetime).time()
			  && showData[showNum].channelID == curRecordings[j].chanid)
			{
				return curRecordings[j].type;
			}
		}
		if (curRecordings[j].type == ScheduledRecording::ChannelRecord)
                {
                        if (showData[showNum].channelID == curRecordings[j].chanid)
                        {
                                return curRecordings[j].type;
                        }
                }
		if (curRecordings[j].type == ScheduledRecording::AllRecord)
                {
			return curRecordings[j].type;
                }
	}
    }
    return 0;

}

void ProgFinder::getRecordingInfo()
{
    QDateTime recDateTime;
    QString thequery;
    QString data;
    thequery = QString("SELECT chanid,starttime,startdate,title,subtitle,description,type "
                       "FROM record ");

    QSqlQuery query = m_db->exec(thequery);

    if (query.numRowsAffected() == -1)
    {
        cerr << "MythProgFind: Error executing query! (getRecordingInfo)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    if (query.numRowsAffected() > 0)
    {

    recordingCount =1;
    curRecordings = new recordingRecord[(int)query.numRowsAffected()];
    recordingCount = 0;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
		recDateTime = QDateTime::fromString(query.value(2).toString() + "T" + query.value(1).toString(),
                                                  Qt::ISODate);

		curRecordings[recordingCount].chanid = query.value(0).toString();
		curRecordings[recordingCount].startdatetime = recDateTime;
		curRecordings[recordingCount].title = QString::fromUtf8(query.value(3).toString());
		curRecordings[recordingCount].subtitle = QString::fromUtf8(query.value(4).toString());
		curRecordings[recordingCount].description = QString::fromUtf8(query.value(5).toString());
		curRecordings[recordingCount].type = query.value(6).toInt();

                if (curRecordings[recordingCount].title == QString::null)
                    curRecordings[recordingCount].title = "";
                if (curRecordings[recordingCount].subtitle == QString::null)
                    curRecordings[recordingCount].subtitle = "";
                if (curRecordings[recordingCount].description == QString::null)
                    curRecordings[recordingCount].description = "";

		recordingCount++;
	}
    }
   
    }
}

void ProgFinder::selectShowData(QString progTitle)
{
    accel->setEnabled(false);
    QDateTime progStart = QDateTime::currentDateTime();
    QDateTime progEnd;
    QString thequery;
    QString data;
    int rectype = 0;

    thequery = QString("SELECT subtitle,starttime,channel.channum,"
		       "channel.callsign,description,endtime,channel.chanid "
                       "FROM program,channel "
                       "WHERE program.title = \"%1\" AND program.chanid = channel.chanid "
		       "AND program.starttime > %2 "
                       "ORDER BY program.subtitle;")
                        .arg(progTitle.utf8()).arg(progStart.toString("yyyyMMddhhmm50"));

    QSqlQuery query = m_db->exec(thequery);

    if (query.numRowsAffected() == -1)
    {
	cerr << "MythProgFind: Error executing query! (selectShowData)\n";
	cerr << "MythProgFind: QUERY = " << thequery << endl;
	return;
    }

    showCount = 0;
 
    if (query.numRowsAffected() < showsPerListing)
    {
	showData = new showRecord[showsPerListing];
	for (int i = 0; i < showsPerListing; i++)
		showData[i].title = "**!0";
    }
    else
    	showData = new showRecord[query.numRowsAffected()];

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
                progStart = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
		progEnd = QDateTime::fromString(query.value(5).toString(),
						  Qt::ISODate);

		// DATE, CHANNEL NUM, CHANNEL CALLSIGN, SUBTITLE, DESCRIPTION, SQL START, SQL END

		showData[showCount].startdatetime = progStart;
		showData[showCount].startDisplay = progStart.toString("ddd M/dd     h:mm ap");
		showData[showCount].endDisplay = progEnd.toString("hh:mm ap");
		showData[showCount].channelNum = query.value(2).toString();
		showData[showCount].channelCallsign = query.value(3).toString();
		showData[showCount].title = progTitle;
		showData[showCount].subtitle = QString::fromUtf8(query.value(0).toString());
		showData[showCount].description = QString::fromUtf8(query.value(4).toString());
		showData[showCount].channelID = query.value(6).toString();
		showData[showCount].starttime = progStart.toString("yyyyMMddhhmm");
		showData[showCount].endtime = progEnd.toString("yyyyMMddhhmm");
		rectype = checkRecordingStatus(showCount);
		showData[showCount].recording = rectype;

                if (showData[showCount].subtitle == QString::null)
                    showData[showCount].subtitle = "";
                if (showData[showCount].description == QString::null)
                    showData[showCount].description = "";

		switch (rectype)
		{
			case ScheduledRecording::SingleRecord:
                            data = "Recording just this showing";
                            break;
                        case ScheduledRecording::TimeslotRecord:
                            data = "Recording when shown in this timeslot";
                            break;
                        case ScheduledRecording::ChannelRecord:
                            data = "Recording when shown on this channel";
                            break;
                        case ScheduledRecording::AllRecord:
                            data = "Recording all showings";
                            break;
                        case ScheduledRecording::NotRecording:
			    data = "Not recording this showing";
                            break;
			default:
			    data = "Error!";
		}
		
		showData[showCount].recText = data;

		/*
                data = progStart.toString("ddd M/dd     h:mm ap") + "~" + 
			progEnd.toString("hh:mm ap") + "~" +
                        query.value(2).toString() + "~" + 
			query.value(3).toString() + "~" +
			query.value(0).toString() + " ~" + 
			query.value(4).toString() + 
			progStart.toString(" ~yyyyMMddhhmm") + 
			progEnd.toString("~yyyyMMddhhmm~") +
			query.value(6).toString();

                showData[showCount] = data;
		*/

                showCount++;
        }
    }

    if (query.numRowsAffected() < showsPerListing)
	showCount = showsPerListing;

    curShow = 0;
    accel->setEnabled(true);
    showShowingList();
}

void ProgFinder::getInitialProgramData()
{
    getRecordingInfo();
    getSearchData(8);
    getSearchData(9);
    getSearchData(11);
    getSearchData(12);

    for (int charNum = 0; charNum < searchCount; charNum++)
    {
	if (charNum == 8)
            charNum = 13;

	getSearchData(charNum);
    }

}

void ProgFinder::getSearchData(int charNum)
{
    int cnts = 0;
    int dataNum = 0;
    int startPlace = 0;
    QDateTime progStart = QDateTime::currentDateTime();
    QString thequery;
    QString data;

    thequery = QString("SELECT title "
                       "FROM program,channel "
                       "WHERE program.title LIKE '%1%' AND program.chanid = channel.chanid "
		       "AND program.starttime > %3 "
		       "GROUP BY program.title "
                       "ORDER BY program.title LIMIT %2;")
			.arg(searchData[charNum]).arg((int)(showsPerListing / 2) + 1).arg(progStart.toString("yyyyMMddhhmm50"));

    QSqlQuery query = m_db->exec(thequery);
 
    if (query.numRowsAffected() == -1)
    {
        cerr << "MythProgFind: Error executing query! (getSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    startPlace = (int)(charNum*showsPerListing);
    dataNum = (int)(showsPerListing / 2);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
		data = QString::fromUtf8(query.value(0).toString());
		
		cnts++;
		initData[startPlace + dataNum] = data;
		dataNum++;
		qApp->processEvents();
	}
    }

    if (query.numRowsAffected() >= (int)(showsPerListing / 2))
    {

    thequery = QString("SELECT title "
                       "FROM program,channel "
                       "WHERE program.title LIKE '%1%' AND program.chanid = channel.chanid "
		       "AND program.starttime > %3 "
                       "GROUP BY program.title "
                       "ORDER BY program.title DESC LIMIT %2;")
                        .arg(searchData[charNum]).arg((int)(showsPerListing / 2)).arg(progStart.toString("yyyyMMddhhmm50"));

    query = m_db->exec(thequery);

    startPlace = (int)(charNum*showsPerListing) + ((int)(showsPerListing / 2) - 1);
    dataNum = 0;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
                data = QString::fromUtf8(query.value(0).toString());

		cnts++;
                initData[startPlace + dataNum] = data;
                dataNum--;
                qApp->processEvents();
        }
    }

    }

    if (cnts == 0)
         gotInitData[charNum] = 1;
    else
    {
         gotInitData[charNum] = 10 + cnts;
    }

    if (charNum == curSearch)
	showSearchList();

}
