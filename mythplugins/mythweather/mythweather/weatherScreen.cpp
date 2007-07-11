#include "weather.h"
#include "weatherScreen.h"
#include "defs.h"
#include <mythtv/mythcontext.h>
using namespace std;


WeatherScreen* WeatherScreen::loadScreen(Weather* parent, LayerSet* container, int id) {

    QString key = container->GetName();
    if (key == "Current Conditions")
        return new CurrCondScreen(parent, container, id);
    if (key == "Three Day Forecast")
        return new ThreeDayForecastScreen(parent, container, id);
    if (key == "Severe Weather Alerts")
        return new SevereWeatherScreen(parent, container, id);
    if (key == "Static Map")
        return new StaticImageScreen(parent, container, id);
   if (key == "Animated Map")
       return new AnimatedImageScreen(parent, container, id);

    return new WeatherScreen(parent, container, id);
}

QStringList WeatherScreen::getAllDynamicTypes(LayerSet* container) {
    vector<UIType*> *types = container->getAllTypes();
    vector<UIType*>::iterator i = types->begin();
    QStringList typesList;
    for(; i < types->end(); ++i) {
        UIType *t = *i;
        if (t->getName().startsWith("+"))
            typesList << t->getName().remove(0,1);
    }
    return typesList;
}



WeatherScreen::WeatherScreen(Weather* parent, LayerSet* container, int id)
{
    m_container = container; 
    m_parent = parent;
    m_id = id;
    
    m_inuse = false;
    vector<UIType*> *types = m_container->getAllTypes();
    vector<UIType*>::iterator i = types->begin();
    for (; i < types->end(); ++i) {
        UIType* t = (UIType*) *i;
        if (t->getName().startsWith("*") || t->getName().startsWith("+"))
            addDataItem(t->getName().remove(0,1), t->getName().startsWith("+"));
    }
}

WeatherScreen::~WeatherScreen() {
    disconnect();
}

bool WeatherScreen::canShowScreen() {
    if (!inUse())
        return false;
    for(uint i = 0; i < map.size(); ++i) {
        if(map[map.keys()[i]] == "NEEDED") {
            VERBOSE(VB_GENERAL, map.keys()[i]);
        }
    }
    
    return !map.values().contains("NEEDED");
}

void WeatherScreen::setValue(QString key, QString value) { 
    
   if (map.contains(key))  
        map[key] = prepareDataItem(key, value); 
}

void WeatherScreen::newData(QString loc, units_t units, DataMap data) {
    (void)loc;
	(void)units;

    DataMap::iterator itr = data.begin();
    while (itr != data.end()) {
        setValue(itr.key(), itr.data());
        ++itr;
    }

    if (canShowScreen()) {
        emit screenReady(this);
    }

}

void WeatherScreen::prepareScreen() {
    QMap<QString,QString>::iterator itr = map.begin();
    while (itr != map.end()) {
        UIType* widget = getType(itr.key());
        if (!widget) {
            VERBOSE(VB_IMPORTANT, "Widget not found " + itr.key());
            ++itr;
            continue;
        }
        if (dynamic_cast<UITextType*> (widget))
            ((UITextType*) widget)->SetText(itr.data());
        else if (dynamic_cast<UIImageType*> (widget)) {
            ((UIImageType*) widget)->SetImage(itr.data());
        } else if (dynamic_cast<UIAnimatedImageType*> (widget)) {
            ((UIAnimatedImageType*) widget)->SetWindow((MythDialog*) m_parent);
            ((UIAnimatedImageType*) widget)->Pause();
            ((UIAnimatedImageType*) widget)->SetFilename(itr.data());
        } else if (dynamic_cast<UIRichTextType*> (widget)) {
            ((UIRichTextType*) widget)->SetText(itr.data());
        }
        prepareWidget(widget);
        ++itr;
    }

}

void WeatherScreen::prepareWidget(UIType* widget) {
    UIImageType* img;
    UIAnimatedImageType* aimg;
    /*
     * Basically so we don't do it twice since some screens (Static Map) mess
     * with image dimensions
     */
    if (img = dynamic_cast<UIImageType*>(widget)) {
        img->LoadImage();
    } else if (aimg = dynamic_cast<UIAnimatedImageType*>(widget)) {
        aimg->LoadImages();
    }
    return;
}

QString WeatherScreen::prepareDataItem(QString key, QString value) {

    (void)key;

    return value;

}

void WeatherScreen::addDataItem(QString item, bool req) {
    if (!map.contains(item))
            map[item] = req ? "NEEDED" : "";
}

void WeatherScreen::draw(QPainter* p) {
    if (m_container) {
        m_container->Draw(p, 0, 0);
        m_container->Draw(p, 1, 0);
        m_container->Draw(p, 2, 0);
        m_container->Draw(p, 3, 0);
        m_container->Draw(p, 4, 0);
        m_container->Draw(p, 5, 0);
        m_container->Draw(p, 6, 0);
        m_container->Draw(p, 7, 0);
        m_container->Draw(p, 8, 0);
    } else {
        VERBOSE(VB_IMPORTANT, "NULL container in WeatherScreen");
    }

}

void WeatherScreen::hiding() {
    pause_animation();
}

void WeatherScreen::showing() {
    prepareScreen();
    unpause_animation();
}

void WeatherScreen::toggle_pause(bool paused) {
    UITextType* txt = (UITextType*) getType("pause_text");
    if (txt) {
        if (paused)
            txt->SetText(tr("- PAUSED -"));
        else
            txt->SetText("");
    }

}

void WeatherScreen::pause_animation() {
    vector<UIType*> *types = m_container->getAllTypes();
    vector<UIType*>::iterator i = types->begin();
    for(; i < types->end(); ++i) {
        UIType *t = *i;
        if (dynamic_cast<UIAnimatedImageType*> (t))
            ((UIAnimatedImageType*) t)->Pause();
    }
}

void WeatherScreen::unpause_animation() {
    vector<UIType*> *types = m_container->getAllTypes();
    vector<UIType*>::iterator i = types->begin();
    for(; i < types->end(); ++i) {
        UIType *t = *i;
        if (dynamic_cast<UIAnimatedImageType*> (t)) {
            ((UIAnimatedImageType*) t)->GotoFirstImage();
            ((UIAnimatedImageType*) t)->UnPause();
        }
    }


}

UIType* WeatherScreen::getType(QString key) {
    if (!m_container)
        return NULL;
    UIType* t = m_container->GetType(key);
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

void WeatherScreen::clock_tick() {

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


CurrCondScreen::CurrCondScreen(Weather* parent, LayerSet* container, int id) 
    : WeatherScreen(parent, container, id)
{
    
}

QString CurrCondScreen::prepareDataItem(QString key, QString value) {
    if (key == "relative_humidity")
        return value + " %";

    if (key == "pressure")
        return value + (m_units == ENG_UNITS ? " in" : " mb");

    if (key == "visibility")
        return value + (m_units == ENG_UNITS ? " mi" : " km");

    if (key == "appt")
        return value == "NA" ? value : value + (m_units == ENG_UNITS ? " F" : " C");

    if (key == "temp")
        return value + (m_units == ENG_UNITS ? " F" : " C");

    if (key == "wind_gust")
        return '('+value+')' + (m_units == ENG_UNITS ? " mph" : " kph");

    return value;

}


ThreeDayForecastScreen::ThreeDayForecastScreen(Weather* parent, LayerSet* container, int id) 
    : WeatherScreen(parent, container, id)
{
}

SevereWeatherScreen::SevereWeatherScreen(Weather* parent, LayerSet* container, int id)
    : WeatherScreen(parent, container, id) 
{
    m_text = (UIRichTextType*) getType("alerts");
    m_text->SetBackground(parent->getBackground());
}


bool SevereWeatherScreen::handleKey(QKeyEvent* e) {
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);

    for (uint i = 0; i < actions.size() && !handled; ++i) {
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

StaticImageScreen::StaticImageScreen(Weather* parent, LayerSet* container, int id) 
    : WeatherScreen(parent, container, id), max(730,450), orgPos(35,100)
{
}

QString StaticImageScreen::prepareDataItem(QString key, QString value) {
    if (key == "map") {
        /*
         * Image value format:
         * /path/to/file-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */
        bool hasdim = (value.findRev('-') > value.findRev('/')); 
        if (hasdim) {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            value = value.left(value.findRev('-'));
            imgsize.setWidth(dim[0].toInt());
            imgsize.setHeight(dim[1].toInt());
            imgsize.scale(max, QSize::ScaleMin);
        } else {
            imgsize = max;
        }
    }

    return value;

}

void StaticImageScreen::prepareWidget(UIType* widget) {
    if (widget->Name() == "+map") {
        /*
         * Scaling the image down and centering it
         */
        UIImageType* img = (UIImageType*) widget;
        int newx = orgPos.x() + (max.width()-imgsize.width())/2;
        int newy = orgPos.y() + (max.height()-imgsize.height())/2;
       
        img->SetSize(imgsize.width(), imgsize.height());
        img->SetPosition(gContext->GetMainWindow()->NormPoint(QPoint(newx, newy)));
        img->LoadImage();
    }
    return;
}

AnimatedImageScreen::AnimatedImageScreen(Weather* parent, LayerSet* container, int id) 
    : WeatherScreen(parent, container, id), max(730,450), orgPos(35,100)
{
}

QString AnimatedImageScreen::prepareDataItem(QString key, QString value) {
    if (key == "animatedimage") {
        /*
         * Image value format:
         * /path/to/file-IMAGECOUNT-WIDTHxHEIGHT
         * if no dimension, scale to max size
         */

        bool hasdim = value.find(QRegExp("-[0-9]{1,}x[0-9]{1,}$"));
        if (hasdim) {
            QStringList dim = QStringList::split('x',
                    value.right(value.length() - value.findRev('-') - 1));
            value = value.left(value.findRev('-'));
            imgsize.setWidth(dim[0].toInt());
            imgsize.setHeight(dim[1].toInt());
            imgsize.scale(max, QSize::ScaleMin);
        } else {
            imgsize = max;
        }

        QString cnt = value.right(value.length() - value.findRev('-') - 1);
        m_count = cnt.toInt();
        value = value.left(value.findRev('-'));

    }

    return value;

}

void AnimatedImageScreen::prepareWidget(UIType* widget) {
    if (widget->Name() == "+animatedimage") {
        /*
         * Scaling the image down and centering it
         */
        UIAnimatedImageType* img = (UIAnimatedImageType*) widget;
        int newx = orgPos.x() + (max.width()-imgsize.width())/2;
        int newy = orgPos.y() + (max.height()-imgsize.height())/2;
       
        img->SetSize(imgsize.width(), imgsize.height());
        img->SetPosition(gContext->GetMainWindow()->NormPoint(QPoint(newx, newy)));
        img->SetImageCount(m_count);
        // TODO this slows things down A LOT!!
        img->LoadImages();
    }
    return;
}
