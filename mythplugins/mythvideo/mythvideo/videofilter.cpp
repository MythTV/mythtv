/*
    editmetadata.cpp
    (c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
    Part of the mythTV project
*/

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "videofilter.h"

VideoFilterSettings::VideoFilterSettings(bool loaddefaultsettings, const QString& _prefix)
{
    if ( !_prefix )
        prefix = "VideoDefault";
    else
        prefix = _prefix + "Default";
    
    // do nothing yet
    if (loaddefaultsettings)
    {
        category = gContext->GetNumSetting( QString("%1Category").arg(prefix),-1);
        genre = gContext->GetNumSetting( QString("%1Genre").arg(prefix),-1);
        country = gContext->GetNumSetting( QString("%1Country").arg(prefix),-1);
        year = gContext->GetNumSetting( QString("%1Year").arg(prefix),-1);
        runtime = gContext->GetNumSetting( QString("%1Runtime").arg(prefix),-2);
        userrating = gContext->GetNumSetting( QString("%1Userrating").arg(prefix),-1);
        browse = gContext->GetNumSetting( QString("%1Browse").arg(prefix),-1);
        orderby = (enum ordering)gContext->GetNumSetting(
                QString("%1Orderby").arg(prefix),kOrderByTitle);
    }
    else
    {
        category = -1;
        genre = -1;
        country = -1;
        year = -1;
        runtime = -2;
        userrating = -1;
        browse = -1;
        orderby = kOrderByTitle;
    }

}

VideoFilterSettings::VideoFilterSettings(VideoFilterSettings *other)
{
    category = other->category;
    genre = other->genre;
    country = other->country;
    year = other->year;
    runtime = other->runtime;
    userrating = other->userrating;
    browse = other->browse;
    orderby = other->orderby;
    prefix = other->prefix;
}

VideoFilterSettings::~VideoFilterSettings()
{
        
}
void VideoFilterSettings::saveAsDefault()
{

    gContext->SaveSetting(QString("%1Category").arg(prefix), category);
    gContext->SaveSetting(QString("%1Genre").arg(prefix), genre);
    gContext->SaveSetting(QString("%1Country").arg(prefix), country);
    gContext->SaveSetting(QString("%1Year").arg(prefix), year);
    gContext->SaveSetting(QString("%1Runtime").arg(prefix), runtime);
    gContext->SaveSetting(QString("%1Userrating").arg(prefix), userrating);
    gContext->SaveSetting(QString("%1Browse").arg(prefix), browse);
    gContext->SaveSetting(QString("%1Orderby").arg(prefix), orderby);
}

VideoFilterDialog::VideoFilterDialog(VideoFilterSettings * settings,
                                 MythMainWindow *parent, 
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    //
    //  The only thing this screen does is let the
    //  user set (some) metadata information. It only
    //  works on a single metadata entry.
    //
    
    originalSettings = settings;
    if (originalSettings){
        //Save data settings before changing them
        currentSettings = new VideoFilterSettings (settings);
    }else{
        currentSettings = new VideoFilterSettings ();
    }

    // Widgets
    year_select = NULL;
    userrating_select = NULL;
    category_select = NULL;
    country_select = NULL;
    genre_select = NULL;
    runtime_select = NULL;
    numvideos_text=NULL;

    wireUpTheme();
    fillWidgets();
    update_numvideo();
    assignFirstFocus();
}
QString VideoFilterSettings::BuildClauseFrom(){
    QString from (" videometadata ");
    if (genre!=-1)
    {
        if (genre==0)
            from = QString("( %1 LEFT JOIN videometadatagenre ON "
                           "videometadata.intid = videometadatagenre.idvideo)").arg(from);
        else
            from = QString("( %1 INNER JOIN videometadatagenre ON "
                           "videometadata.intid = videometadatagenre.idvideo)").arg(from);
    }
    
    if (country!=-1)
    {
        if (country==0)
            from = QString("( %1 LEFT JOIN videometadatacountry ON "
                           "videometadata.intid = videometadatacountry.idvideo)").arg(from);
        else
            from = QString("( %1 INNER JOIN videometadatacountry ON "
                           "videometadata.intid = videometadatacountry.idvideo)").arg(from);
    }
    return from;
}

QString VideoFilterSettings::BuildClauseWhere()
{
    QString where = NULL;
    if (genre!=-1)
    {
        QString condition;
        if (genre ==0 )
            condition = QString(" IS NULL");
        else
            condition = QString(" = %1").arg(genre);
        where = QString(" WHERE videometadatagenre.idgenre %1 ").arg(condition);
    }
    
    if (country!=-1)
    {
        QString condition;
        if (country==0)
            condition = QString(" IS NULL");
        else
            condition = QString(" = %1").arg(country);
        
        if (where)
            where +=  QString(" AND videometadatacountry.idcountry %1 ").arg(condition);
        else
            where = QString(" WHERE videometadatacountry.idcountry %1 ").arg(condition);
    }
    
    if (category != -1)
    {
        if (where)
            where += QString(" AND category = %1").arg(category);
        else
            where = QString(" WHERE category = %1").arg(category);
    }
    
    if (year!=-1)
    {
        if (where)
            where += QString(" AND year = %1").arg(year);
        else
            where = QString(" WHERE year = %1").arg(year);
    }
    
    if (runtime != -2)
    {
        if (where)
            where += QString(" AND FLOOR((length-1)/30) = %1").arg(runtime);
        else
            where = QString(" WHERE FLOOR((length-1)/30) = %1").arg(runtime);
    }
    
    if (userrating !=-1)
    {
        if (where)
            where += QString(" AND userrating >= %1").arg(userrating);
        else 
            where = QString(" WHERE userrating >= %1").arg(userrating);
    }

    if (browse !=-1)
    {
        if (where)
            where += QString(" AND browse = %1").arg(browse);
        else 
            where = QString(" WHERE browse = %1").arg(browse);
    } 
    

    return where;
}

QString VideoFilterSettings::BuildClauseOrderBy()
{
    switch (orderby)
    {
        case kOrderByTitle : 
            return " ORDER BY title";
        case kOrderByYearDescending : 
            return " ORDER BY year DESC";
        case kOrderByUserRatingDescending : 
            return " ORDER BY userrating DESC";
        case kOrderByLength : 
            return " ORDER BY length";
        default:
            return "";        
    }
}

void VideoFilterDialog::update_numvideo()
{
    
    if (numvideos_text)
    {
        QString select = QString("SELECT NULL FROM ");
        QString from = currentSettings->BuildClauseFrom();
        QString where = currentSettings->BuildClauseWhere();
        QString q_string = QString("%1 %2 %3")
                           .arg(select).arg(from).arg(where);
        

        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);
        
        if((a_query.isActive()) && (a_query.size()>0))
        {
            numvideos_text->SetText(
                    QString(tr("Result of this filter : %1 video(s)"))
                            .arg(a_query.size()));
        }
        else
        {
            numvideos_text->SetText(QString(
                            tr("Result of this filter : No Videos")));
        }
    }
}

void VideoFilterDialog::fillWidgets()
{
    if (category_select)
    {
        category_select->addItem(-1, "All");
        QString q_string = QString("SELECT intid, category FROM videocategory "
                                   "ORDER BY category");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);

        if (a_query.isActive() && a_query.size()>0)
        {
            while (a_query.next())
            {
                QString cat = QString::fromUtf8(a_query.value(1).toString());
                category_select->addItem(a_query.value(0).toInt(), cat);
            }
        }
        category_select->addItem(0,tr("Unknown"));
        category_select->setToItem(currentSettings->getCategory());

    }
    if (genre_select)
    {
        genre_select->addItem(-1,"All");
        QString q_string = QString("Select intid, genre FROM videogenre "
                                   "INNER JOIN videometadatagenre "
                                   "ON intid = idgenre "
                                   "GROUP BY intid , genre "
                                   "ORDER BY genre;");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);

        if (a_query.isActive() && a_query.size()>0)
        {
            while (a_query.next())
            {
                QString genre = QString::fromUtf8(a_query.value(1).toString());
                genre_select->addItem(a_query.value(0).toInt(), genre);
            }
        }
        genre_select->addItem(0,tr("Unknown"));
        genre_select->setToItem(currentSettings->getGenre());
    }
    
    if (country_select)
    {
        country_select->addItem(-1,"All");
        QString q_string = QString("Select intid, country FROM videocountry "
                                   "INNER JOIN videometadatacountry "
                                   "ON intid = idcountry "
                                   "GROUP BY intid, country "
                                   "ORDER BY country;");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);
        if (a_query.isActive() && a_query.size()>0)
        {
            while(a_query.next())
            {
                QString country = QString::fromUtf8(a_query.value(1).toString());
                country_select->addItem(a_query.value(0).toInt(), country);
            }
        }
        country_select->addItem(0,tr("Unknown"));
        country_select->setToItem(currentSettings->getCountry());
    }

    if(year_select)
    {
        year_select->addItem(-1,"All");
        QString q_string = QString("SELECT year FROM videometadata "
                                   "GROUP BY year ORDER BY year DESC;");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);

        if(a_query.isActive() && a_query.size()>0)
        {
            while(a_query.next())
            {
                if (a_query.value(0).toInt() == 0) 
                {
                    year_select->addItem(0,tr("Unknown"));
                }
                else
                {
                    year_select->addItem(a_query.value(0).toInt(),
                                         a_query.value(0).toString());
                }
            }
        }
        year_select->setToItem(currentSettings->getYear());

    }
    
    if (runtime_select)
    {
        runtime_select->addItem(-2,"All");
        QString q_string = QString("SELECT FLOOR((length-1)/30) "
                                   "FROM videometadata "
                                   "GROUP BY FLOOR((length-1)/30);");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);

        if (a_query.isActive() && a_query.size()>0)
        {
           while (a_query.next())
           {
                if (a_query.value(0).toInt()<0)
                {
                    runtime_select->addItem(a_query.value(0).toInt(),tr("Unknown"));
                }
                else
                {
                    QString s = QString("%1 ").arg(a_query.value(0).toInt()*30);
                    s += tr("minutes");
                    s += " ~ " + QString("%1 ").arg((a_query.value(0).toInt()+1)*30);
                    s += tr("minutes");
                    runtime_select->addItem(a_query.value(0).toInt(), s);
                }
           }
        }
        runtime_select->setToItem(currentSettings->getRuntime());
   }

    if (userrating_select)
    {
        userrating_select->addItem(-1,tr("All"));
        QString q_string = QString("SELECT FLOOR(userrating) "
                                   "FROM videometadata "
                                   "GROUP BY FLOOR(userrating) DESC;");
        MSqlQuery a_query(MSqlQuery::InitCon());
        a_query.exec(q_string);

        if(a_query.isActive()&&a_query.size()>0)
        {
            while(a_query.next())
            {
                userrating_select->addItem(a_query.value(0).toInt(),
                                           ">= " + a_query.value(0).toString());
            }
        }
        userrating_select->setToItem(currentSettings->getUserrating());
    }
    
    if (browse_select)
    {
        browse_select->addItem(-1,"All");
        browse_select->addItem(1,"Yes");
        browse_select->addItem(0,"No");
        browse_select->setToItem(currentSettings->getBrowse());
    }
    
    if (orderby_select)
    {
        orderby_select->addItem(0,"title");
        orderby_select->addItem(1,"year");
        orderby_select->addItem(2,"userrating");
        orderby_select->addItem(3,"runtime");
        orderby_select->setToItem(currentSettings->getOrderby());
    }
}

void VideoFilterDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool something_pushed = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if ((action == "LEFT") || (action == "RIGHT"))
        {
            something_pushed = false;
            UISelectorType *currentSelector = NULL;
           if ((category_select)&&(getCurrentFocusWidget() == category_select)) 
                currentSelector = category_select;
           if ((genre_select)&&(getCurrentFocusWidget() == genre_select))
                currentSelector = genre_select;
            if ((country_select)&&(getCurrentFocusWidget() == country_select))
                currentSelector = country_select;
           if ((year_select) && (getCurrentFocusWidget() == year_select)) 
                currentSelector = year_select;
           if ((runtime_select)&&(getCurrentFocusWidget() == runtime_select))
                currentSelector = runtime_select;   
           if ((userrating_select)&&(getCurrentFocusWidget() == userrating_select)) 
                currentSelector = userrating_select;
            if ((browse_select)&&(getCurrentFocusWidget() == browse_select)) 
                currentSelector = browse_select;
            if ((orderby_select)&&(getCurrentFocusWidget() == orderby_select))
                currentSelector = orderby_select;
            if(currentSelector)
            {
                currentSelector->push(action=="RIGHT");
                something_pushed = true;
            }
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "SELECT")
            activateCurrent();
        else if (action == "0")
        {    
            if (done_button)
                done_button->push();
        }
        else
            handled = false;
    }
    
    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}


void VideoFilterDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);
    
    MythRemoteLineEdit *which_editor = (MythRemoteLineEdit *)sender();

    if(which_editor)
    {
        which_editor->clearFocus();
    }
}
void VideoFilterDialog::saveAsDefault()
{
     currentSettings->saveAsDefault();
     this->saveAndExit();
}


void VideoFilterDialog::saveAndExit()
{
    if (originalSettings)
    {
        originalSettings->setCategory(currentSettings->getCategory());
        originalSettings->setGenre(currentSettings->getGenre());
        originalSettings->setCountry(currentSettings->getCountry());
        originalSettings->setYear(currentSettings->getYear());
        originalSettings->setRuntime(currentSettings->getRuntime());
        originalSettings->setUserrating(currentSettings->getUserrating());
        originalSettings->setBrowse(currentSettings->getBrowse());
        originalSettings->setOrderby(currentSettings->getOrderby());
    }
    done(0);
}

void VideoFilterDialog::setYear(int new_year)
{
        currentSettings->setYear(new_year);
        update_numvideo();
}


void VideoFilterDialog::setUserRating(int new_userrating)
{
        currentSettings->setUserrating(new_userrating);
        update_numvideo();
}


void VideoFilterDialog::setCategory(int new_category)
{
        currentSettings->setCategory(new_category);
        update_numvideo();
}


void VideoFilterDialog::setCountry(int new_country)
{
        currentSettings->setCountry(new_country);
        update_numvideo();
}

void VideoFilterDialog::setGenre(int new_genre)
{
        currentSettings->setGenre(new_genre);
        update_numvideo();
}


void VideoFilterDialog::setRunTime(int new_runtime)
{
        currentSettings->setRuntime(new_runtime);
        update_numvideo();
}


void VideoFilterDialog::setBrowse(int new_browse)
{
        currentSettings->setBrowse(new_browse);
        update_numvideo();
}


void VideoFilterDialog::setOrderby(int new_orderby)
{
        currentSettings->setOrderby(
                (enum VideoFilterSettings::ordering)new_orderby);
        update_numvideo();
}


void VideoFilterDialog::wireUpTheme()
{
    year_select = getUISelectorType("year_select");
    if(year_select)
        connect(year_select, SIGNAL(pushed(int)),
                this, SLOT(setYear(int)));

    userrating_select = getUISelectorType("userrating_select");
    if (userrating_select)
        connect(userrating_select, SIGNAL(pushed(int)),
                this, SLOT(setUserRating(int)));

    category_select = getUISelectorType("category_select");
    if (category_select)
        connect(category_select, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));

    country_select = getUISelectorType("country_select");
    if (country_select)
        connect(country_select, SIGNAL(pushed(int)),
                this, SLOT(setCountry(int)));

    genre_select = getUISelectorType("genre_select");
    if (genre_select)
        connect(genre_select,SIGNAL(pushed(int)),
                this, SLOT(setGenre(int)));

    runtime_select = getUISelectorType("runtime_select");
    if (runtime_select)
        connect(runtime_select, SIGNAL(pushed(int)),
                this, SLOT(setRunTime(int)));

   
    browse_select = getUISelectorType("browse_select");
    if (browse_select)
        connect(browse_select, SIGNAL(pushed(int)),
                this, SLOT(setBrowse(int)));
   

    orderby_select = getUISelectorType("orderby_select");
    if (orderby_select)
        connect(orderby_select, SIGNAL(pushed(int)),
                this, SLOT(setOrderby(int)));

    save_button = getUITextButtonType("save_button");
    
    if (save_button)
    {
        save_button->setText(tr("Save as default"));
        connect(save_button, SIGNAL(pushed()), this, SLOT(saveAsDefault()));
    }
  
    done_button = getUITextButtonType("done_button");
    if(done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }
    
    numvideos_text = getUITextType("numvideos_text");
    buildFocusList();
}


VideoFilterDialog::~VideoFilterDialog()
{
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
