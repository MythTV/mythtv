#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
#include <qnetwork.h>
#include <qurl.h>
#include <qdir.h>

using namespace std;

#include "metadata.h"
#include "videobrowser.h"
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>

VideoBrowser::VideoBrowser(QSqlDatabase *ldb,
                           MythMainWindow *parent, const char *name)
            : MythDialog(parent, name)
{
    db = ldb;
    updateML = false;
    currentParentalLevel = gContext->GetNumSetting("VideoDefaultParentalLevel", 4);

    RefreshMovieList();

    noUpdate = false;
    m_state = 0;

    curitem = NULL;
    inData = 0;

    fullRect = QRect(0, 0, (int)(800*wmult), (int)(600*hmult));

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "browser", "video-");
    LoadWindow(xmldata);

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    SetCurrentItem();
    updateBackground();

    setNoErase();
}

VideoBrowser::~VideoBrowser()
{
    delete theme;
    delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void VideoBrowser::keyPressEvent(QKeyEvent *e)
{ 
    if (allowselect)
    {
        switch (e->key())
        {
            case Key_Space: case Key_Enter: case Key_Return: 
                selected(curitem); return;
            default: break;
        }
    }

    switch (e->key())
    {
        case Key_Down: cursorDown(); break;
        case Key_Up: cursorUp(); break;
        case Key_Left: cursorLeft(); break;
        case Key_Right: cursorRight(); break;
        default: MythDialog::keyPressEvent(e);
    }
}

void VideoBrowser::updateBackground(void)
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

void VideoBrowser::RefreshMovieList()
{
    if (updateML == true)
        return;
    updateML = true;
    m_list.clear();

    QSqlQuery query("SELECT intid FROM videometadata ORDER BY title;", db);
    Metadata *myData;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
           unsigned int idnum = query.value(0).toUInt();

           myData = new Metadata();
           myData->setID(idnum);
           myData->fillDataFromID(db);
           if (myData->ShowLevel() <= currentParentalLevel && myData->ShowLevel() != 0)
               m_list.append(*myData);

           delete myData;
        }
    }
    updateML = false;
}

void VideoBrowser::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), (int)(600*hmult));
}

void VideoBrowser::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (m_state == 0)
    {
       if (r.intersects(infoRect) && noUpdate == false)
       {
           updateInfo(&p);
       }
       if (r.intersects(browsingRect) && noUpdate == false)
       {
           updateBrowsing(&p);
       }
    }
    else if (m_state > 0)
    {
        noUpdate = true;
        updatePlayWait(&p);
    }
}

void VideoBrowser::updatePlayWait(QPainter *p)
{
  if (m_state < 4)
  {
    backup.flush();
    backup.begin(this);
    if (m_state == 1)
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
    m_state++;
    update(fullRect);
  }
  else if (m_state == 4)
  {
    // Play the movie
    myth_system((QString("%1 ") .arg(m_cmd)).local8Bit());

    Metadata *childItem = new Metadata;
    Metadata *parentItem = new Metadata(*curitem);

    while (parentItem->ChildID())
    {
        childItem->setID(parentItem->ChildID());
        childItem->fillDataFromID(db);

        if (parentItem->ChildID())
        {
            //Load up data about this child
            selected(childItem);
            myth_system((QString("%1 ") .arg(m_cmd)).local8Bit());
        }

        delete parentItem;
        parentItem = new Metadata(*childItem);
    }

    delete childItem;
    delete parentItem;

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();
    noUpdate = false;

    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    gContext->GetMainWindow()->currentWidget()->setFocus();

    m_state = 0;
    update(fullRect);
  }
}


void VideoBrowser::SetCurrentItem()
{
    ValueMetadata::Iterator it;
    ValueMetadata::Iterator start;
    ValueMetadata::Iterator end;
    start = m_list.begin();
    end = m_list.end();
    int cnt = 0;

    for (it = start; it != end; ++it)
    {
         if (cnt == inData)
         {
             if (curitem)
                 delete curitem;
             curitem = new Metadata(*(it));
         }
         cnt++;
    }
}

void VideoBrowser::updateBrowsing(QPainter *p)
{
    QRect pr = browsingRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
  
    QString vidnum = QString("%1 of %2").arg(inData + 1).arg(m_list.count());
    QString plevel = QString("%1").arg(currentParentalLevel);

    LayerSet *container = NULL;
    container = theme->GetSet("browsing");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("currentvideo");
        if (type)
            type->SetText(vidnum);

        type = (UITextType *)container->GetType("currentlevel");
        if (type)
            type->SetText(plevel);
  
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

void VideoBrowser::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (m_list.count() > 0 && curitem)
    {
       QString title = curitem->Title();
       QString filename = curitem->Filename();
       QString director = curitem->Director();
       QString year = QString("%1").arg(curitem->Year());
       if (year == "1895") 
           year = "?";
       QString coverfile = curitem->CoverFile();
       QString inetref = curitem->InetRef();
       QString plot = curitem->Plot();
       QString userrating = QString("%1").arg(curitem->UserRating());
       QString rating = curitem->Rating();
       if (rating == "<NULL>")
           rating = "No rating available.";
       QString length = QString("%1").arg(curitem->Length()) + " minutes";
       QString level = QString("%1").arg(curitem->ShowLevel());

       LayerSet *container = NULL;
       container = theme->GetSet("info");
       if (container)
       {
           UITextType *type = (UITextType *)container->GetType("title");
           if (type)
               type->SetText(title);

           type = (UITextType *)container->GetType("filename");
           if (type)
               type->SetText(filename);

           type = (UITextType *)container->GetType("director");
           if (type)
               type->SetText(director);
 
           type = (UITextType *)container->GetType("year");
           if (type)
               type->SetText(year);

           type = (UITextType *)container->GetType("coverfile");
           if (type)
               type->SetText(coverfile);
  
           UIImageType *itype = (UIImageType *)container->GetType("coverart");
           if (itype)
           {
               itype->SetImage(coverfile);
               itype->LoadImage();
           }

           type = (UITextType *)container->GetType("inetref");
           if (type)
               type->SetText(inetref);

           type = (UITextType *)container->GetType("plot");
           if (type)
               type->SetText(plot);
 
           type = (UITextType *)container->GetType("userrating");
           if (type)
               type->SetText(userrating);

           type = (UITextType *)container->GetType("rating");
           if (type)
               type->SetText(rating);

           type = (UITextType *)container->GetType("length");
           if (type)
               type->SetText(length);

           type = (UITextType *)container->GetType("level");
           if (type)
               type->SetText(level);
  
           container->Draw(&tmp, 1, 0); 
           container->Draw(&tmp, 2, 0);
           container->Draw(&tmp, 3, 0);
           container->Draw(&tmp, 4, 0);  
           container->Draw(&tmp, 5, 0);
           container->Draw(&tmp, 6, 0); 
           container->Draw(&tmp, 7, 0);
           container->Draw(&tmp, 8, 0);
       }

       allowselect = true;
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

       allowselect = false;
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

}

void VideoBrowser::LoadWindow(QDomElement &element)
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

void VideoBrowser::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "info")
        infoRect = area;
    if (name.lower() == "browsing")
        browsingRect = area;
}
 
void VideoBrowser::exitWin()
{
    emit accept();
}

void VideoBrowser::cursorLeft()
{
     inData--;
     if (inData < 0)
         inData = m_list.count() - 1;
     SetCurrentItem();
     update(infoRect);
     update(browsingRect);
}

void VideoBrowser::cursorRight()
{
     inData++;
     if (inData >= (int)m_list.count())
         inData = 0;
     SetCurrentItem();
     update(infoRect);
     update(browsingRect);
}

void VideoBrowser::cursorUp()
{
    currentParentalLevel++;
    if (currentParentalLevel > 4)
        currentParentalLevel = 4;
    inData = 0;
    RefreshMovieList();
    SetCurrentItem();
    update(infoRect);
    update(browsingRect);
}

void VideoBrowser::cursorDown()
{
    currentParentalLevel--;
    if (currentParentalLevel < 1)
        currentParentalLevel = 1;
    inData = 0;
    RefreshMovieList();
    SetCurrentItem();
    update(infoRect);
    update(browsingRect);
}

void VideoBrowser::selected(Metadata *someItem)
{
    QString filename = someItem->Filename();
    QString ext = someItem->Filename().section('.',-1);

    QString handler = gContext->GetSetting("VideoDefaultPlayer");

    //
    //  Do we have a specialized player for this
    //  type of file?
    //
        
    QString extension = filename.section(".", -1, -1);

    QString q_string = QString("SELECT playcommand, use_default FROM "
                               "videotypes WHERE extension = \"%1\" ;")
                               .arg(extension);

    QSqlQuery a_query(q_string, db);
    
    if(a_query.isActive() && a_query.numRowsAffected() > 0)
    {
        a_query.next();
        if(!a_query.value(1).toBool())
        {
            //
            //  This file type is defined and
            //  it is not set to use default player
            //
            handler = a_query.value(0).toString();                
        }
    }

    QString arg;
    arg.sprintf("\"%s\"",
                filename.replace(QRegExp("\""), "\\\"").utf8().data());

    QString command = "";
    
    if(handler.contains("%s"))
    {
        command = handler.replace(QRegExp("%s"), arg);
    }
    else
    {
        command = handler + " " + arg;
    }

    //cout << "command:" << command << endl;

    m_title = someItem->Title();
    LayerSet *container = NULL;
    container = theme->GetSet("playwait");
    if (container)
    {
         UITextType *type = (UITextType *)container->GetType("title");
         if (type)
             type->SetText(m_title);
    }
    m_cmd = command;
    m_state = 1;
    update(fullRect);
}
