#include <qlayout.h>
#include <qlistview.h>
#include <qaccel.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
#include <qscrollview.h> 

using namespace std;

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "dirlist.h"
#include <mythtv/mythcontext.h>

DatabaseBox::DatabaseBox(QSqlDatabase *ldb, QValueList<Metadata> *movielist, 
                         QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    db = ldb;
    m_list = new QValueList<Metadata>;
    *m_list = *movielist;

    curitem = NULL;

    pageDowner = false;

    inList = 0;
    inData = 0;
    listCount = 0;
    dataCount = 0;
    m_status = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    accel = new QAccel(this);

    space_itemid = accel->insertItem(Key_Space);
    enter_itemid = accel->insertItem(Key_Enter);
    return_itemid = accel->insertItem(Key_Return);

    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));
    accel->connectItem(space_itemid, this, SLOT(selected()));
    accel->connectItem(enter_itemid, this, SLOT(selected()));
    accel->connectItem(return_itemid, this, SLOT(selected()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(exitWin()));

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "video", "video-");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythVideo: DatabaseBox : Failed to get selector object.\n";
        exit(0);
    }

    bgTransBackup = new QPixmap();
    resizeImage(bgTransBackup, "trans-backup.png");

    updateBackground();

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);

    showFullScreen();
    setActiveWindow();
}

DatabaseBox::~DatabaseBox()
{
    delete theme;
    delete accel;
    delete bgTransBackup;
    if (m_list)
        delete m_list;
    if (curitem)
        delete curitem;
}

void DatabaseBox::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void DatabaseBox::resizeImage(QPixmap *dst, QString file)
{
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
         file = themeDir + file;
    else
         file = baseDir + file;

    if (hmult == 1 && wmult == 1)
    {
         dst->load(file);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            scalerImg = sourceImg->smoothScale((int)(sourceImg->width() * wmult),
                                               (int)(sourceImg->height() * hmult));
            dst->convertFromImage(scalerImg);
        }
        delete sourceImg;
    }
}

void DatabaseBox::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), (int)(600*hmult));
}

void DatabaseBox::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(listRect) && m_status == 0)
    {
        updateList(&p);
    }
    if (r.intersects(infoRect) && m_status == 0) 
    {
        updateInfo(&p);
    }
    if (r.intersects(fullRect) && m_status > 0)
    {
        updatePlayWait(&p);
    }
}

void DatabaseBox::updatePlayWait(QPainter *p)
{
  if (m_status < 3)
  {
    backup.flush();
    backup.begin(this);
    if (m_status == 1)
        grayOut(&backup);
    backup.end();

    LayerSet *container = NULL;
    container = theme->GetSet("playwait");
    if (container)
    {
        container->Draw(p, 0, 0);
        container->Draw(p, 1, 0);
        container->Draw(p, 2, 0);
        container->Draw(p, 3, 0);
    }
    m_status++;
    update(fullRect);
  }
  else if (m_status == 3)
  {
    system((QString("%1 ") .arg(m_cmd)).ascii());

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();
 
    raise();
    setActiveWindow();

    m_status = 0;
  }
}

void DatabaseBox::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    QString filename = "";
    QString title = "";

    QValueList<Metadata>::Iterator it;
    QValueList<Metadata>::Iterator start;
    QValueList<Metadata>::Iterator end;

    start = m_list->begin();
    end = m_list->end();

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("listing");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (it = start; it != end; ++it)
            {
               if (cnt < listsize)
               {
                  if (pastSkip <= 0)
                  {
                      filename =(*it).Filename();
                      if (0 == (*it).Title().compare("title"))
                      {
                          title = filename.section('/', -1);
                          if (!gContext->GetNumSetting("ShowFileExtensions"))
                          title = title.section('.',0,-2);
                      }
                      else
                          title = (*it).Title();

                      if (cnt == inList)
                      {
                          if (curitem)
                              delete curitem;
                          curitem = new Metadata(*(it));
                          ltype->SetItemCurrent(cnt);
                      }

                      ltype->SetItemText(cnt, 1, title);

                      if ((*it).Genre() == "dir")
                      {
                          ltype->EnableForcedFont(cnt, "directory");
                      }

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

    dataCount = m_list->count();

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

void DatabaseBox::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (m_list->count() > 0 && curitem)
    {
       QString title = "";
       QString filename = curitem->Filename();
       QString genre = "";

       title = filename.section('/', -1);
       if (!gContext->GetNumSetting("ShowFileExtensions"))
           title = title.section('.',0,-2);

       if (curitem->Genre() != "(null)")
           genre = curitem->Genre();
       else
           genre = "";

       LayerSet *container = NULL;
       container = theme->GetSet("program_info");
       if (container)
       {
           UITextType *type = (UITextType *)container->GetType("title");
           if (type)
               type->SetText(title);

           type = (UITextType *)container->GetType("filename");
           if (type)
               type->SetText(filename);

           type = (UITextType *)container->GetType("genre");
           if (type)
               type->SetText(genre);
       } 

    }
    else
    {
       LayerSet *norec = theme->GetSet("novideos_info");
       if (norec)
       {
           norec->Draw(&tmp, 4, 0);
           norec->Draw(&tmp, 5, 0);
           norec->Draw(&tmp, 6, 0);
           norec->Draw(&tmp, 7, 0);
           norec->Draw(&tmp, 8, 0);
       }

       accel->setItemEnabled(space_itemid, false);
       accel->setItemEnabled(enter_itemid, false);
       accel->setItemEnabled(return_itemid, false);

    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

}

void DatabaseBox::LoadWindow(QDomElement &element)
{

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void DatabaseBox::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "video_info")
        infoRect = area;
}

void DatabaseBox::exitWin()
{
    emit killTheApp();
}

void DatabaseBox::cursorDown(bool page)
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
    else if ((int)(inData + listsize) >= (int)(dataCount))
    {
        inData = dataCount - listsize;
    }
    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void DatabaseBox::cursorUp(bool page)
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


void DatabaseBox::selected()
{
    Metadata *mdata = curitem;

    if (mdata != NULL && mdata->Genre() != "dir")
    {
        QString filename = mdata->Filename();	  
        QString title = mdata->Filename().section('/',-1);
        QString ext = mdata->Filename().section('.',-1);
	 
        QString handler = gContext->GetSetting(QString("%1_helper").arg(ext.lower()));
 	//cout << "handler for" << ext.ascii() << ":" << handler.ascii() << endl;
        QString command = handler.replace(QRegExp("%s"), QString("\"%1\"")
                                .arg(filename.replace(QRegExp("\""), "\\\"")));
 //	cout << "command:" << command << endl;

        m_title = title;
        LayerSet *container = NULL;
        container = theme->GetSet("playwait");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(m_title);
        }
        m_cmd = command;
        m_status = 1;

        update(fullRect);
    }
    else if (mdata != NULL && mdata->Genre() == "dir")
    {
//        printf("got directory %s\n",mdata->Filename().ascii());
        QString dirname = mdata->Filename();
        Dirlist md(dirname);
           
        QValueList<Metadata> new_list = md.GetPlaylist();

        if (m_list)
            delete m_list;

        m_list = new QValueList<Metadata>;
        *m_list = new_list;
  
        inList = 0;
        inData = 0;
        listCount = 0;
        dataCount = 0;
        
        update(fullRect);
    }
}
