#include <vector>

#include <mythtv/mythcontext.h>

#include "weather.h"
#include "weatherScreen.h"
#include "defs.h"
//Added by qt3to4:
#include <QPixmap>
#include <QKeyEvent>

WeatherScreen *WeatherScreen::loadScreen(Weather *parent, LayerSet *container,
                                         int id)
{
    QString key = container->GetName();
    if (key == "Current Conditions")
        return new CurrCondScreen(parent, container, id);
    if (key == "Three Day Forecast")
        return new ThreeDayForecastScreen(parent, container, id);
    if (key == "Six Day Forecast")
        return new SixDayForecastScreen(parent, container, id);
    if (key == "Severe Weather Alerts")
        return new SevereWeatherScreen(parent, container, id);
    if (key == "Static Map")
        return new StaticImageScreen(parent, container, id);
   if (key == "Animated Map")
       return new AnimatedImageScreen(parent, container, id);

    return new WeatherScreen(parent, container, id);
}

QStringList WeatherScreen::getAllDynamicTypes(LayerSet *container)
{
    vector<UIType *> *types = container->getAllTypes();
    vector<UIType *>::iterator i = types->begin();
    QStringList typesList;
    for (; i < types->end(); ++i)
    {
        UIType *t = *i;
        if (t->getName().startsWith("+"))
            typesList << t->getName().remove(0, 1);
    }
    return typesList;
}

WeatherScreen::WeatherScreen(Weather *parent, LayerSet *container, int id)
{
    m_container = container;
    m_parent = parent;
    m_id = id;
    m_prepared = false;

    m_inuse = false;
    vector<UIType *> *types = m_container->getAllTypes();
    vector<UIType *>::iterator i = types->begin();
    for (; i < types->end(); ++i)
    {
        UIType *t = (UIType *) *i;
        if (t->getName().startsWith("*") || t->getName().startsWith("+"))
            addDataItem(t->getName().remove(0, 1),
                        t->getName().startsWith("+"));
    }
}

WeatherScreen::~WeatherScreen()
{
    disconnect();
}

bool WeatherScreen::canShowScreen()
{
    if (!inUse())
        return false;

    for (uint i = 0; i < map.size(); ++i)
    {
        if (map[map.keys()[i]] == "NEEDED")
        {
            VERBOSE(VB_GENERAL, map.keys()[i]);
        }
    }

    return !map.values().contains("NEEDED");
}

void WeatherScreen::setValue(const QString &key, const QString &value)
{
    if (map.contains(key))
        map[key] = prepareDataItem(key, value);
}

void WeatherScreen::newData(QString loc, units_t units, DataMap data)
{
    (void) loc;
    (void) units;

    DataMap::iterator itr = data.begin();
    while (itr != data.end())
    {
        setValue(itr.key(), itr.data());
        ++itr;
    }

    if (canShowScreen())
    {
        emit screenReady(this);
    }
}

void WeatherScreen::prepareScreen()
{
    QMap<QString, QString>::iterator itr = map.begin();
    while (itr != map.end())
    {
        UIType *widget = getType(itr.key());
        if (!widget)
        {
            VERBOSE(VB_IMPORTANT, "Widget not found " + itr.key());
            ++itr;
            continue;
        }

        if (dynamic_cast<UITextType *>(widget))
            ((UITextType *) widget)->SetText(itr.data());
        else if (dynamic_cast<UIImageType *>(widget))
        {
            ((UIImageType *) widget)->SetImage(itr.data());
        }
        else if (dynamic_cast<UIAnimatedImageType *>(widget))
        {
            ((UIAnimatedImageType *) widget)->SetWindow((MythDialog *) m_parent);
            ((UIAnimatedImageType *) widget)->Pause();
            ((UIAnimatedImageType *) widget)->SetFilename(itr.data());
        }
        else if (dynamic_cast<UIRichTextType *>(widget))
        {
            ((UIRichTextType *) widget)->SetText(itr.data());
        }

        prepareWidget(widget);
        ++itr;
    }

    m_prepared = true;
}

void WeatherScreen::prepareWidget(UIType *widget)
{
    UIImageType *img;
    UIAnimatedImageType *aimg;
    /*
     * Basically so we don't do it twice since some screens (Static Map) mess
     * with image dimensions
     */
    if ((img = dynamic_cast<UIImageType *>(widget)))
    {
        img->LoadImage();
    }
    else if ((aimg = dynamic_cast<UIAnimatedImageType *>(widget)))
    {
        aimg->LoadImages();
    }
}

QString WeatherScreen::prepareDataItem(const QString &key, const QString &value)
{
    (void) key;
    return value;
}

void WeatherScreen::addDataItem(const QString &item, bool required)
{
    if (!map.contains(item))
        map[item] = required ? "NEEDED" : "";
}

void WeatherScreen::draw(QPainter *p)
{
    if (m_container)
    {
        QRect area = m_container->GetAreaRect();
        if (area.isNull())
        {
            VERBOSE(VB_IMPORTANT, QString("Container %1 has NULL area, "
                                          "bad theme.")
                                          .arg(m_container->GetName()));
            area.setWidth(800);
            area.setHeight(600);
        }
        QPixmap pix(area.size());
        pix.fill(m_parent, area.topLeft());
        QPainter tmp(&pix);
        for (int i = 0; i < 9; ++i)
            m_container->Draw(&tmp, i, 0);
        tmp.end();
        p->drawPixmap(area.topLeft(), pix);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "NULL container in WeatherScreen");
    }
}

void WeatherScreen::hiding()
{
    pause_animation();
}

void WeatherScreen::showing()
{
    if (!m_prepared)
        prepareScreen();
    unpause_animation();
}

void WeatherScreen::toggle_pause(bool paused)
{
    UITextType *txt = (UITextType *) getType("pause_text");
    if (txt)
    {
        QString pausetext = QString("- %1 -").arg(tr("PAUSED"));
        if (paused)
            txt->SetText(pausetext);
        else
            txt->SetText("");
    }
}

void WeatherScreen::pause_animation()
{
    vector<UIType *> *types = m_container->getAllTypes();
    for (vector<UIType *>::iterator i = types->begin(); i < types->end(); ++i)
    {
        UIAnimatedImageType *t = dynamic_cast<UIAnimatedImageType *>(*i);
        if (t) t->Pause();
    }
}

void WeatherScreen::unpause_animation()
{
    vector<UIType *> *types = m_container->getAllTypes();
    for (vector<UIType *>::iterator i = types->begin(); i < types->end(); ++i)
    {
        UIAnimatedImageType *t = dynamic_cast<UIAnimatedImageType *>(*i);
        if (t)
        {
            t->GotoFirstImage();
            t->UnPause();
        }
    }
}

UIType *WeatherScreen::getType(const QString &key)
{
    if (!m_container)
        return NULL;

    UIType *t = m_container->GetType(key);
    if (t)
        return t;

    t = m_container->GetType("*" + key);
    if (t)
        return t;

    t = m_container->GetType("+" + key);
    if (t)
        return t;

    return NULL;
}

void WeatherScreen::clock_tick()
{
    QDateTime new_time(QDateTime::currentDateTime());
    QString curDate;
    if (QString(gContext->GetSetting("Language")) == "JA")
        curDate = new_time.toString("M/d (ddd) h:mm ap");
    else
        curDate = new_time.toString("MMM d h:mm ap");

    curDate = new_time.date().toString(Qt::LocalDate);
    curDate += new_time.time().toString(" h:mm ap");
    setValue("currentdatetime", curDate);
}

CurrCondScreen::CurrCondScreen(Weather *parent, LayerSet *container, int id)
    : WeatherScreen(parent, container, id)
{
}

QString CurrCondScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key == "relative_humidity")
        return value + " %";

    if (key == "pressure")
        return value + (m_units == ENG_UNITS ? " in" : " mb");

    if (key == "visibility")
        return value + (m_units == ENG_UNITS ? " mi" : " km");

    if (key == "appt")
        return value == "NA" ? value : value + (m_units == ENG_UNITS ? "°F" : "°C");

    if (key == "temp")
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }

    if (key == "wind_gust" || key == "wind_spdgst" || key == "wind_speed")
        return value + (m_units == ENG_UNITS ? " mph" : " kph");

    return value;
}

ThreeDayForecastScreen::ThreeDayForecastScreen(Weather *parent,
                                               LayerSet *container, int id) :
    WeatherScreen(parent, container, id)
{
}

QString ThreeDayForecastScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key.contains("low",FALSE) || key.contains("high",FALSE) )
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }
    
    return value;
}

SixDayForecastScreen::SixDayForecastScreen(Weather *parent,
                                               LayerSet *container, int id) :
    WeatherScreen(parent, container, id)
{
}

QString SixDayForecastScreen::prepareDataItem(const QString &key,
                                        const QString &value)
{
    if (key.contains("low",FALSE) || key.contains("high",FALSE) )
    {
       if ( (value == "NA") || (value == "N/A") )
          return value;
       else
          return value + (m_units == ENG_UNITS ? "°F" : "°C");
    }
    
    return value;
}

SevereWeatherScreen::SevereWeatherScreen(Weather *parent, LayerSet *container,
                                         int id) :
    WeatherScreen(parent, container, id)
{
    m_text = (UIRichTextType *) getType("alerts");
    m_text->SetBackground(parent->getBackground());
}

bool SevereWeatherScreen::handleKey(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);

    for (uint i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            m_text->ScrollUp();
        else if (action == "DOWN")
            m_text->ScrollDown();
        else if (action == "PAGEUP")
            m_text->ScrollPageUp();
        else if (action == "PAGEDOWN")
            m_text->ScrollPageDown();
        else handled = false;
    }
    m_parent->update();

    return handled;
}

StaticImageScreen::StaticImageScreen(Weather *parent, LayerSet *container,
                                     int id) :
    WeatherScreen(parent, container, id)
{
}

QString StaticImageScreen::prepareDataItem(const QString &key,
                                           const QString &value)
{
    QString ret = value;
    if (key == "map")
    {
        /*
         * Image value format:
         * /path/to/file-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */
        bool hasdim = (value.findRev('-') > value.findRev('/'));
        if (hasdim)
        {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            ret = value.left(value.findRev('-'));
            imgsize.setWidth(dim[0].toInt());
            imgsize.setHeight(dim[1].toInt());
        }
    }

    return ret;
}

void StaticImageScreen::prepareWidget(UIType *widget)
{
    if (widget->Name() == "+map")
    {
        /*
         * Scaling the image down and centering it
         */
        UIImageType *img = (UIImageType *) widget;
        QSize max = img->GetSize();

        if (imgsize.width() > -1 && imgsize.height() > -1)
        {
            if (max.width() > -1 && max.height() > -1)
            {
                imgsize.scale(max, Qt::KeepAspectRatio);

                QPoint orgPos = img->DisplayPos();

                int newx = orgPos.x() + (max.width() - imgsize.width()) / 2;
                int newy = orgPos.y() + (max.height() - imgsize.height()) / 2;
                img->SetPosition(QPoint(newx, newy));
            }
            img->SetSize(imgsize.width(), imgsize.height());
        }
        img->LoadImage();
    }
}

AnimatedImageScreen::AnimatedImageScreen(Weather *parent, LayerSet *container,
                                         int id) :
    WeatherScreen(parent, container, id)
{
}

QString AnimatedImageScreen::prepareDataItem(const QString &key,
                                             const QString &value)
{
    QString ret = value;
    if (key == "animatedimage")
    {
        /*
         * Image value format:
         * /path/to/file-IMAGECOUNT-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */

        bool hasdim = value.find(QRegExp("-[0-9]{1,}x[0-9]{1,}$"));
        if (hasdim)
        {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            ret = value.left(value.findRev('-'));
            imgsize.setWidth(dim[0].toInt());
            imgsize.setHeight(dim[1].toInt());
        }

        QString cnt = ret.right(ret.length() - ret.findRev('-') - 1);
        m_count = cnt.toInt();
        ret = ret.left(ret.findRev('-'));
    }

    return ret;
}

void AnimatedImageScreen::prepareWidget(UIType *widget)
{
    if (widget->Name() == "+animatedimage")
    {
        /*
         * Scaling the image down and centering it
         */
        UIAnimatedImageType *img = (UIAnimatedImageType *) widget;
        QSize max = img->GetSize();

        if (imgsize.width() > -1 && imgsize.height() > -1)
        {
            if (max.width() > -1 && max.height() > -1)
            {
                imgsize.scale(max, Qt::KeepAspectRatio);

                QPoint orgPos = img->DisplayPos();

                int newx = orgPos.x() + (max.width() - imgsize.width()) / 2;
                int newy = orgPos.y() + (max.height() - imgsize.height()) / 2;
                img->SetPosition(QPoint(newx, newy));
            }
            img->SetSize(imgsize.width(), imgsize.height());
        }

        img->SetImageCount(m_count);
        // TODO this slows things down A LOT!!
        img->LoadImages();
    }
    return;
}
