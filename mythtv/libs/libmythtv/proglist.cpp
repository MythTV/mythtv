#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qheader.h>

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

#include "proglist.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

ProgLister::ProgLister(const QString &ltitle, QSqlDatabase *ldb, 
		   MythMainWindow *parent, const char *name)
            : MythDialog(parent, name)
{
    title = ltitle;
    db = ldb;
    curTime = QDateTime::currentDateTime();
    timeFormat = gContext->GetSetting("ShortDateFormat") +
	" " + gContext->GetSetting("TimeFormat");

    progList.setAutoDelete(true);
    dataCount = 0;
    curProg = -1;

    doingSel = false;
    allowKeys = true;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);

    if (!theme->LoadTheme(xmldata, "programlist"))
    {
        DialogBox diag(gContext->GetMainWindow(), "The theme you are using "
                       "does not contain a 'programlist' element.  Please "
                       "contact the theme creator and ask if they could "
                       "please update it.<br><br>The next screen will be empty."
                       "  Escape out of it to return to the menu.");
        diag.AddButton("OK");
        diag.exec();

        return;
    }

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("proglist");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythFrontEnd: ProgLister - Failed to get selector object.\n";
        exit(0);
    }

    updateBackground();

    fillProgList();

    pageDowner = false;
    inList = 0;
    inData = 0;
    setNoErase();

    gContext->addListener(this);
}

ProgLister::~ProgLister()
{
    gContext->removeListener(this);
    delete theme;
}

void ProgLister::keyPressEvent(QKeyEvent *e)
{
    if (!allowKeys)
        return;

    switch (e->key())
    {
        case Key_Left: case Key_Right: break;
        case Key_Up: cursorUp(false); break;
        case Key_Down: cursorDown(false); break;
        case Key_PageUp: case Key_9: cursorUp(true); break;
        case Key_PageDown: case Key_3: cursorDown(true); break;
	case Key_Space: case Key_Enter: case Key_Return:
                select(); break;
        case Key_I: case Key_M:  edit(); break;
        case Key_R: quickRecord(); break;
        case Key_Escape: done(0); break;
        default: /*MythDialog::keyPressEvent(e);*/ break;
    }
}

void ProgLister::LoadWindow(QDomElement &element)
{
    QString name;
    int context;
    QRect area;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
                theme->parseFont(e);
            else if (e.tagName() == "container")
	    {
		theme->parseContainer(e, name, context, area);
		if (name.lower() == "selector")
		    listRect = area;
		if (name.lower() == "program_info")
		    infoRect = area;
	    }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void ProgLister::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
	container->Draw(&tmp, 0, 0);

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void ProgLister::paintEvent(QPaintEvent *e)
{
    if (doingSel)
        return;

    QRect r = e->rect();
    QPainter p(this);
 
    if (r.intersects(listRect))
        updateList(&p);
    if (r.intersects(infoRect))
        updateInfo(&p);
}

void ProgLister::cursorDown(bool page)
{
    if (page == false)
    {
        if (inList > (int)((int)(listsize / 2) - 1)
            && ((int)(inData + listsize) <= (int)(dataCount - 1))
            && pageDowner == true)
        {
            inData++;
            inList = (int)(listsize / 2);
        }
        else
        {
            inList++;

            if (inList >= listCount)
                inList = listCount - 1;
        }
    }
    else if (page == true && pageDowner == true)
    {
        if (inList >= (int)(listsize / 2) || inData != 0)
        {
            inData = inData + listsize;
        }
        else if (inList < (int)(listsize / 2) && inData == 0)
        {
            inData = (int)(listsize / 2) + inList;
            inList = (int)(listsize / 2);
        }
    }
    else if (page == true && pageDowner == false)
    {
        inList = listsize - 1;
    }

    if ((int)(inData + inList) >= (int)(dataCount))
    {
        inData = dataCount - listsize;
        inList = listsize - 1;
    }
    else if ((int)(inData + listsize) >= (int)dataCount)
    {
        inData = dataCount - listsize;
    }

    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void ProgLister::cursorUp(bool page)
{
    if (page == false)
    {
        if (inList < ((int)(listsize / 2) + 1) && inData > 0)
        {
            inList = (int)(listsize / 2);
            inData--;
            if (inData < 0)
	    {
                inData = 0;
                inList--;
            }
	}
	else
        {
	    inList--;
        }
    }
    else if (page == true && inData > 0)
    {
	inData = inData - listsize;
	if (inData < 0)
        {
	    inList = inList + inData;
	    inData = 0;
	    if (inList < 0)
		inList = 0;
        }

	if (inList > (int)(listsize / 2))
        {
	    inList = (int)(listsize / 2);
	    inData = inData + (int)(listsize / 2) - 1;
        }
    }
    else if (page == true)
    {
	inData = 0;
	inList = 0;
    }

    if (inList > -1)
    {
	update(fullRect);
    }
    else
	inList = 0;
}

void ProgLister::quickRecord()
{
    ProgramInfo *pi = progList.at(curProg);

    pi->ToggleRecord(db);

    fillProgList();
    update(fullRect);
}

void ProgLister::select()
{
    ProgramInfo *pi = progList.at(curProg);

    doingSel = true;

    allowKeys = false;
    pi->EditRecording(db);
    allowKeys = true;

    fillProgList();
    update(fullRect);

    doingSel = false;
}

void ProgLister::edit()
{
    ProgramInfo *pi = progList.at(curProg);

    doingSel = true;

    allowKeys = false;
    pi->EditScheduled(db);
    allowKeys = true;

    fillProgList();
    update(fullRect);

    doingSel = false;
}

void ProgLister::fillProgList(void)
{
    QString where = QString("WHERE program.title = \"%1\" AND "
			    "program.chanid = channel.chanid "
			    "AND program.endtime > %2 "
			    "ORDER BY program.starttime,channel.channum;")
	                    .arg(title.utf8())
	                    .arg(curTime.toString("yyyyMMddhhmm50"));

    allowKeys = false;

    progList.clear();
    ProgramInfo::GetProgramListByQuery(db, &progList, where);
    dataCount = progList.count();

    ProgramInfo *p;
    vector<ProgramInfo *> recList;
    RemoteGetAllPendingRecordings(recList);

    for (p = progList.first(); p; p = progList.next())
        p->FillInRecordInfo(recList);

    while (!recList.empty())
    {
        p = recList.back();
        delete p;
        recList.pop_back();
    }

    allowKeys = true;
}

void ProgLister::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = inData;
    pageDowner = false;
    listCount = 0;

    ProgramInfo *pi;

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("proglist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (int i = 0; i < dataCount; i++)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        pi = progList.at(i);

                        if (cnt == inList)
                        {
                            curProg = i;
                            ltype->SetItemCurrent(cnt);
                        }

			ltype->SetItemText(cnt, 1, pi->startts.toString(timeFormat));
                        ltype->SetItemText(cnt, 2, pi->chanstr + " " + pi->chansign);
			if (pi->subtitle == "")
			    ltype->SetItemText(cnt, 3, title);
			else
			    ltype->SetItemText(cnt, 3, pi->subtitle);
                        if (pi->conflicting)
                            ltype->EnableForcedFont(cnt, "conflicting");
                        else if (pi->recording)
                            ltype->EnableForcedFont(cnt, "recording");

                        cnt++;
                        listCount++;
                    }
                    pastSkip--;
                }
                else
                    pageDowner = true;
            }
        }

        ltype->SetDownArrow(pageDowner);
        if (inData > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
    }

    if (dataCount <= 0)
        container = theme->GetSet("noprograms_list");

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ProgLister::updateInfo(QPainter *p)
{
    int rectyperank, chanrank, progrank, totalrank;
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    RecordingType rectype; 

    if (dataCount > 0)
    {  
        progrank = 0;
        rectype = kSingleRecord;
        rectyperank = 0;
        chanrank = 0;
        totalrank = progrank + rectyperank + chanrank;
        LayerSet *container;
	ProgramInfo *pi = progList.at(curProg);

        container = theme->GetSet("program_info");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(title);
 
            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(pi->subtitle);

            type = (UITextType *)container->GetType("type");
            if (type)
                type->SetText(pi->RecordingText());

            type = (UITextType *)container->GetType("description");
            if (type) {
                type->SetText(pi->description);
            }
        }
       
        if (container)
        {
            container->Draw(&tmp, 4, 0);
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
            container->Draw(&tmp, 7, 0);
            container->Draw(&tmp, 8, 0);
        }
    }
    else
    {
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (norec)
        {
            norec->Draw(&tmp, 4, 0);
            norec->Draw(&tmp, 5, 0);
            norec->Draw(&tmp, 6, 0);
            norec->Draw(&tmp, 7, 0);
            norec->Draw(&tmp, 8, 0);
        }
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

