#include "settings.h"
#include "mythwidgets.h"
#include <qsqldatabase.h>
#include <qlineedit.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qlcdnumber.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qvgroupbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qwidgetstack.h>
#include <qtabdialog.h>
#include <qfile.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qwidget.h>
#include <unistd.h>
#include <qdatetime.h>
#include <qdir.h>

#include "mythcontext.h"
#include "mythwizard.h"
#include "mythdbcon.h"
#include "DisplayRes.h"

/** \class Configurable
 *  \brief Configurable is the root of all the database aware widgets.
 *
 *   This is an abstract class and some methods must be implemented
 *   in children. byName(const &QString) is abstract. While 
 *   configWidget(ConfigurationGroup *, QWidget*, const char*)
 *   has an implementation, all it does is print an error message
 *   and return a NULL pointer.
 */

QWidget* Configurable::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) 
{
    (void)cg;
    (void)parent;
    (void)widgetName;
    VERBOSE(VB_IMPORTANT, "BUG: Configurable is visible, but has no "
            "configWidget");
    return NULL;
}

/** \fn Configurable::enableOnSet(const QString &val)
 *  \brief This slot allows you to enable this configurable when a
 *         binary configurable is set to true.
 *  \param val signal value, should be "0" to disable, other to disable.
 */
void Configurable::enableOnSet(const QString &val)
{
    setEnabled( val != "0" );
}

/** \fn Configurable::enableOnUnset(const QString &val)
 *  \brief This slot allows you to enable this configurable when a
 *         binary configurable is set to false.
 *  \param val signal value, should be "0" to enable, other to disable.
 */
void Configurable::enableOnUnset(const QString &val)
{
    setEnabled( val == "0" );
}

QString Setting::getValue(void) const
{
    return QDeepCopy<QString>(settingValue);
}

void Setting::setValue(const QString &newValue)
{
    settingValue = QDeepCopy<QString>(newValue);
    SetChanged(true);
    emit valueChanged(settingValue);
}

ConfigurationGroup::~ConfigurationGroup()
{
    for (childList::iterator it = children.begin(); it != children.end() ; ++it)
    {
        if (*it)
            (*it)->deleteLater();
    }
    children.clear();
}

void ConfigurationGroup::deleteLater(void)
{
    for (childList::iterator it = children.begin(); it != children.end() ; ++it)
    {
        if (*it)
            (*it)->disconnect();
    }
}

Setting *ConfigurationGroup::byName(const QString &name)
{
    Setting *tmp = NULL;

    childList::iterator it = children.begin();
    for (; !tmp && (it != children.end()); ++it)
    {
        if (*it)
            tmp = (*it)->byName(name);
    }
    
    return tmp;
}

void ConfigurationGroup::load(void)
{
    childList::iterator it = children.begin();
    for (; it != children.end() ; ++it)
        if (*it && (*it)->GetStorage())
            (*it)->GetStorage()->load();
}

void ConfigurationGroup::save(void)
{
    childList::iterator it = children.begin();
    for (; it != children.end() ; ++it)
        if (*it && (*it)->GetStorage())
            (*it)->GetStorage()->save();
}

void ConfigurationGroup::save(QString destination)
{
    childList::iterator it = children.begin();
    for (; it != children.end() ; ++it)
        if (*it && (*it)->GetStorage())
            (*it)->GetStorage()->save(destination);
}

QWidget* VerticalConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                  QWidget* parent,
                                                  const char* widgetName) 
{    
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    float wmult = 0, hmult = 0;
    gContext->GetScreenSettings(wmult, hmult);

    int space  = (zeroSpace) ? 4 : -1;
    int margin = (int) ((uselabel) ? (28 * hmult) : (10 * hmult));
    margin = (zeroMargin) ? 4 : margin;

    QVBoxLayout *layout = new QVBoxLayout(widget, margin, space);

    if (uselabel)
        widget->setTitle(getLabel());

    for (uint i = 0; i < children.size(); i++)
    {
        if (children[i]->isVisible())
        {
            QWidget *child = children[i]->configWidget(cg, widget, NULL);
            layout->add(child);
            children[i]->setEnabled(children[i]->isEnabled());
        }
    }
      
    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)),
                cg,   SIGNAL(changeHelpText(QString)));
    } 

    return widget;
}

QWidget* HorizontalConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                    QWidget* parent, 
                                                    const char* widgetName) 
{
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    QHBoxLayout *layout = NULL;

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    if (uselabel)
    {
        int space = -1;
        int margin = (int)(28 * hmult);
        if (zeroSpace)
            space = 4;
        if (zeroMargin)
            margin = 4;

        layout = new QHBoxLayout(widget, margin, space);
        // This makes weird and bad things happen in qt -mdz 2002/12/28
        //widget->setInsideMargin(20);
        widget->setTitle(getLabel());
    }
    else
    {
        int space = -1;
        int margin = (int)(10 * hmult);
        if (zeroSpace)
            space = 4;
        if (zeroMargin)
            margin = 4;

        layout = new QHBoxLayout(widget, margin, space);
    }

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
        {
            QWidget *child = children[i]->configWidget(cg, widget, NULL);
            layout->add(child);
            children[i]->setEnabled(children[i]->isEnabled());
        }

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
}

QWidget* GridConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                              QWidget* parent, 
                                              const char* widgetName)
{
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    QGridLayout *layout = NULL;

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    int rows = (children.size()+columns-1) / columns;
    if (uselabel)
    {
        int space = -1;
        int margin = (int)(28 * hmult);
        if (zeroSpace)
            space = 4;
        if (zeroMargin)
            margin = 4;

        layout = new QGridLayout(widget, rows, columns, margin, space);
        // This makes weird and bad things happen in qt -mdz 2002/12/28
        //widget->setInsideMargin(20);
        widget->setTitle(getLabel());
    }
    else
    {
        int space = -1;
        int margin = (int)(10 * hmult);
        if (zeroSpace)
            space = 4;
        if (zeroMargin)
            margin = 4;

        layout = new QGridLayout(widget, rows, columns, margin, space);
    }

    for (unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
        {
            QWidget *child = children[i]->configWidget(cg, widget, NULL);
            layout->addWidget(child, i / columns, i % columns);
            children[i]->setEnabled(children[i]->isEnabled());
        }

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
}

QWidget* StackedConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                 QWidget* parent,
                                                 const char* widgetName) 
{
    QWidgetStack* widget = new QWidgetStack(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addWidget(children[i]->configWidget(cg, widget,
                                                        NULL), i);

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
}

QWidget* TabbedConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                QWidget* parent,
                                                const char* widgetName) 
{
    QTabDialog* widget = new QTabDialog(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addTab(children[i]->configWidget(cg, widget), 
                           children[i]->getLabel());

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
};

void StackedConfigurationGroup::raise(Configurable* child) {
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i] == child) {
            top = i;
            emit raiseWidget((int)i);
            return;
        }
    cout << "BUG: StackedConfigurationGroup::raise(): unrecognized child " << child << " on setting " << getName() << '/' << getLabel() << endl;
}

void StackedConfigurationGroup::save(void)
{
    if (saveAll)
        ConfigurationGroup::save();
    else if (top < children.size())
        children[top]->GetStorage()->save();
}

void StackedConfigurationGroup::save(QString destination)
{
    if (saveAll)
        ConfigurationGroup::save(destination);
    else if (top < children.size())
        children[top]->GetStorage()->save(destination);
}

void TriggeredConfigurationGroup::addChild(Configurable* child)
{
    VerifyLayout();
    configLayout->addChild(child);
}

void TriggeredConfigurationGroup::addTarget(QString triggerValue,
                                            Configurable *target)
{
    VerifyLayout();
    triggerMap[triggerValue] = target;

    if (!configStack)
    {
        configStack = new StackedConfigurationGroup(
            stackUseLabel, stackUseFrame, stackZeroMargin, stackZeroSpace);
        configStack->setSaveAll(isSaveAll);
    }

    configStack->addChild(target);
}

Setting *TriggeredConfigurationGroup::byName(const QString &settingName)
{
    VerifyLayout();
    Setting *setting = ConfigurationGroup::byName(settingName);

    if (!setting)
        setting = configLayout->byName(settingName);

    if (!setting && !widget)
        setting = configStack->byName(settingName);

    return setting;
}

void TriggeredConfigurationGroup::load(void)
{
    VerifyLayout();

    configLayout->load();

    if (!widget)
        configStack->load();
}

void TriggeredConfigurationGroup::save(void)
{
    VerifyLayout();

    configLayout->save();

    if (!widget)
        configStack->save();
}

void TriggeredConfigurationGroup::save(QString destination)
{
    VerifyLayout();

    configLayout->save(destination);

    if (!widget)
        configStack->save(destination);
}

void TriggeredConfigurationGroup::setTrigger(Configurable *_trigger)
{
    if (trigger)
    {
        trigger->disconnect();
    }

    trigger = _trigger;

    if (trigger)
    {
        connect(trigger, SIGNAL(valueChanged(  const QString&)),
                this,    SLOT(  triggerChanged(const QString&)));
    }
}

/** \fn TriggeredConfigurationGroup::SetVertical(bool)
 *  \brief By default we use a vertical layout, but you can call this
 *         with a false value to use a horizontal layout instead.
 *
 *  NOTE: This must be called before this addChild() is first called.
 */
void TriggeredConfigurationGroup::SetVertical(bool vert)
{
    if (configLayout)
    {
        VERBOSE(VB_IMPORTANT, "TriggeredConfigurationGroup::setVertical(): "
                "Sorry, this must be called before any children are added "
                "to the group.");
        return;
    }

    isVertical = vert;
}

void TriggeredConfigurationGroup::VerifyLayout(void)
{
    if (configLayout)
        return;

    if (isVertical)
    {
        configLayout = new VerticalConfigurationGroup(
            uselabel, useframe, zeroMargin, zeroSpace);
    }
    else
    {
        configLayout = new HorizontalConfigurationGroup(
            uselabel, useframe, zeroMargin, zeroSpace);
    }

    ConfigurationGroup::addChild(configLayout);
}

QWidget *TriggeredConfigurationGroup::configWidget(
    ConfigurationGroup *cg, QWidget *parent, const char *widgetName)
{
    VerifyLayout();

    configLayout->addChild(configStack);

    widget = configLayout->configWidget(cg, parent, widgetName);

    return widget;
}

void SelectSetting::addSelection(const QString& label, QString value, bool select) {
    if (value == QString::null)
        value = label;
    
    bool found = false;
    for(unsigned i = 0 ; i < values.size() ; ++i)
        if ((values[i] == value) &&
            (labels[i] == label)) {
            found = true;
            break;
        }

    if (!found)
    {
        labels.push_back(label);
        values.push_back(value);
    
        emit selectionAdded(label, value);
    
        if (select || !isSet)
            setValue(value);
    }
}

void SelectSetting::fillSelectionsFromDir(const QDir& dir, bool absPath) {
     const QFileInfoList *il = dir.entryInfoList();
     if (!il)
         return;

     QFileInfoListIterator it( *il );
     QFileInfo *fi;

     for(; (fi = it.current()) != 0; ++it)
         if (absPath)
             addSelection(fi->absFilePath());
         else
             addSelection(fi->fileName());
}

void SelectSetting::clearSelections(void) {
    labels.clear();
    values.clear();
    isSet = false;
    emit selectionsCleared();
}

void SelectSetting::setValue(const QString& newValue)  {
    bool found = false;
    for(unsigned i = 0 ; i < values.size() ; ++i)
        if (values[i] == newValue) {
            current = i;
            found = true;
            isSet = true;
            break;
        }
    if (found)
        Setting::setValue(newValue);
    else
        addSelection(newValue, newValue, true);
}

void SelectSetting::setValue(int which)
{
    if ((unsigned)which > values.size()-1 || which < 0) {
        VERBOSE(VB_IMPORTANT, 
                "SelectSetting::setValue(): invalid index " << which);
        return;
    }
    current = which;
    isSet = true;
    Setting::setValue(values[current]);
}

QWidget* LabelSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    (void)cg;

    QWidget* widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QLabel* value = new QLabel(widget);
    value->setText(getValue());
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* LineEditSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char *widgetName) {
    QHBox *widget;

    if (labelAboveWidget)
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    }
    else
        widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    edit = new MythLineEdit(settingValue, widget,
                                          QString(widgetName) + "-edit");
    edit->setHelpText(getHelpText());
    edit->setBackgroundOrigin(QWidget::WindowOrigin);
    edit->setText( getValue() );

    connect(this, SIGNAL(valueChanged(const QString&)),
            edit, SLOT(setText(const QString&)));
    connect(edit, SIGNAL(textChanged(const QString&)),
            this, SLOT(setValue(const QString&)));

    if (cg)
        connect(edit, SIGNAL(changeHelpText(QString)), cg, 
                SIGNAL(changeHelpText(QString)));

    edit->setRW(rw);

    return widget;
}

void LineEditSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (edit)
        edit->setEnabled(b);
}

void LineEditSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (edit)
    {
        //QWidget *parent = edit->parentWidget();
        if (b)
            edit->show();
        else
            edit->hide();
    }
}

QWidget* SliderSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                     const char* widgetName) {
    QHBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new QHBox(parent, widgetName);

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    MythSlider* slider = new MythSlider(widget, 
                                        QString(widgetName) + "-slider");
    slider->setHelpText(getHelpText());
    slider->setMinValue(min);
    slider->setMaxValue(max);
    slider->setOrientation(QSlider::Horizontal);
    slider->setLineStep(step);
    slider->setValue(intValue());
    slider->setBackgroundOrigin(QWidget::WindowOrigin);

    QLCDNumber* lcd = new QLCDNumber(widget, QString(widgetName) + "-lcd");
    lcd->setMode(QLCDNumber::DEC);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->display(intValue());

    connect(slider, SIGNAL(valueChanged(int)), lcd, SLOT(display(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

    if (cg)
        connect(slider, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return widget;
}

QWidget* SpinBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName) {
    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        label->setText(getLabel() + ":     ");
    }

    MythSpinBox* spinbox = new MythSpinBox(box, QString(widgetName) + 
                                           "MythSpinBox", sstep);
    spinbox->setHelpText(getHelpText());
    spinbox->setBackgroundOrigin(QWidget::WindowOrigin);
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    // only set step size if greater than default (1), otherwise
    // this will screw up the single-step/jump behavior of the MythSpinBox
    if (1 < step)
        spinbox->setLineStep(step);
    spinbox->setValue(intValue());
    if (!svtext.isEmpty())
        spinbox->setSpecialValueText(svtext);

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), spinbox, SLOT(setValue(int)));

    if (cg)
        connect(spinbox, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return box;
}

QWidget* SelectLabelSetting::configWidget(ConfigurationGroup *cg,
                                          QWidget* parent,
                                          const char* widgetName) {
    (void)cg;

    QHBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new QHBox(parent, widgetName);

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QLabel* value = new QLabel(widget);
    value->setText(labels[current]);
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* ComboBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName) {
    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        box->setStretchFactor(label, 0);
    }

    widget = new MythComboBox(rw, box);
    widget->setHelpText(getHelpText());
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    box->setStretchFactor(widget, 1);

    for(unsigned int i = 0 ; i < labels.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);

    if (1 < step)
        widget->setStep(step);

    if (rw)
        connect(widget, SIGNAL(highlighted(const QString &)),
                this, SLOT(setValue(const QString &)));
    else
        connect(widget, SIGNAL(highlighted(int)),
                this, SLOT(setValue(int)));

    connect(widget, SIGNAL(destroyed()),
            this, SLOT(widgetDestroyed()));
    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return box;
}

void ComboBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (widget)
        widget->setEnabled(b);
}

void ComboBoxSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (widget)
    {
        //QWidget *parent = widget->parentWidget();
        if (b)
            widget->show();
        else
            widget->hide();
    }
}

void ComboBoxSetting::setValue(QString newValue)
{
    if (!rw)
    {
        VERBOSE(VB_IMPORTANT, "ComboBoxSetting::setValue(QString): "
                "BUG: attempted to set value of read-only ComboBox");
        return;
    }
    Setting::setValue(newValue);
    if (widget)
        widget->setCurrentItem(current);
};

void ComboBoxSetting::setValue(int which)
{
    if (widget)
        widget->setCurrentItem(which);
    SelectSetting::setValue(which);
};

void HostRefreshRateComboBox::ChangeResolution(const QString& resolution)
{
    clearSelections();
    
    const vector<short> list = GetRefreshRates(resolution);
    addSelection(QObject::tr("Any"), "0");
    int hz50 = -1, hz60 = -1;
    for (uint i=0; i<list.size(); ++i)
    {        
        QString sel = QString::number(list[i]);
        addSelection(sel+" Hz", sel);
        hz50 = (50 == list[i]) ? i : hz50;
        hz60 = (60 == list[i]) ? i : hz60;
    }
    
    setValue(0);
    if ("640x480" == resolution || "720x480" == resolution)
        setValue(hz60+1);
    if ("640x576" == resolution || "720x576" == resolution)
        setValue(hz50+1);
    
    setEnabled(list.size());
}

const vector<short> HostRefreshRateComboBox::GetRefreshRates(const QString &res)
{
    QStringList slist = QStringList::split("x", res);
    int w = 0, h = 0;
    bool ok0 = false, ok1 = false;
    if (2 == slist.size())
    {
        w = slist[0].toInt(&ok0);
        h = slist[1].toInt(&ok1);
    }

    DisplayRes *display_res = DisplayRes::GetDisplayRes();
    if (display_res && ok0 && ok1)
        return display_res->GetRefreshRates(w, h);    

    vector<short> list;
    return list;
}

void PathSetting::addSelection(const QString& label,
                               QString value,
                               bool select) {
    QString pathname = label;
    if (value != QString::null)
        pathname = value;

    if (mustexist && !QFile(pathname).exists())
        return;

    ComboBoxSetting::addSelection(label, value, select);
}

QTime TimeSetting::timeValue(void) const {
    return QTime::fromString(getValue(), Qt::ISODate);
}

void TimeSetting::setValue(const QTime& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

QWidget* TimeSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                   const char* widgetName) {
    //QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    (void)cg;
    (void)parent;
    (void)widgetName;
    return NULL;
}

QDate DateSetting::dateValue(void) const {
    return QDate::fromString(getValue(), Qt::ISODate);
}

void DateSetting::setValue(const QDate& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

QWidget* DateSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                   const char* widgetName) {
    //QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    (void)cg;
    (void)parent;
    (void)widgetName;
    return NULL;
}

QWidget* RadioSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    QButtonGroup* widget = new QButtonGroup(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setTitle(getLabel());

    for( unsigned i = 0 ; i < labels.size() ; ++i ) {
        QRadioButton* button = new QRadioButton(widget, NULL);
        button->setBackgroundOrigin(QWidget::WindowOrigin);
        button->setText(labels[i]);
        if (isSet && i == current)
            button->setDown(true);
    }

    cg = cg;

    return widget;
}

QWidget* CheckBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName) {
    widget = new MythCheckBox(parent, widgetName);
    widget->setHelpText(getHelpText());
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setText(getLabel());
    widget->setChecked(boolValue());

    connect(widget, SIGNAL(toggled(bool)),
            this, SLOT(setValue(bool)));
    connect(this, SIGNAL(valueChanged(bool)),
            widget, SLOT(setChecked(bool)));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return widget;
}

void CheckBoxSetting::setEnabled(bool fEnabled)
{
    BooleanSetting::setEnabled(fEnabled);
    if (widget)
        widget->setEnabled(fEnabled);
}

void ConfigurationDialogWidget::keyPressEvent(QKeyEvent* e) 
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "SELECT")
                accept();
            else if (action == "ESCAPE")
                reject();
            else if (action == "EDIT")
                emit editButtonPressed();
            else if (action == "DELETE")
                emit deleteButtonPressed();
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

MythDialog* ConfigurationDialog::dialogWidget(MythMainWindow *parent,
                                              const char *widgetName) 
{
    dialog = new ConfigurationDialogWidget(parent, widgetName);

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    QVBoxLayout *layout = new QVBoxLayout(dialog, (int)(20 * hmult));

    ChildList::iterator it = cfgChildren.begin();
    for (; it != cfgChildren.end(); ++it)
    {
        if ((*it)->isVisible())
            layout->addWidget((*it)->configWidget(cfgGrp, dialog));
    }

    return dialog;
}

int ConfigurationDialog::exec(bool saveOnAccept, bool doLoad) 
{
    if (doLoad)
        load();

    MythDialog *dialog = dialogWidget(
        gContext->GetMainWindow(), "Configuration Dialog");

    dialog->Show();

    int ret = dialog->exec();

    if ((QDialog::Accepted == ret) && saveOnAccept)
        save();

    dialog->deleteLater();
    dialog = NULL;

    return ret;
}

void ConfigurationDialog::addChild(Configurable *child)
{
    cfgChildren.push_back(child);
    cfgGrp->addChild(child);
}

MythDialog *ConfigurationWizard::dialogWidget(MythMainWindow *parent,
                                              const char     *widgetName)
{
    MythWizard *wizard = new MythWizard(parent, widgetName);
    dialog = wizard;

    QObject::connect(cfgGrp, SIGNAL(changeHelpText(QString)),
                     wizard, SLOT(  setHelpText(   QString)));

    QWidget *child = NULL;
    ChildList::iterator it = cfgChildren.begin();
    for (; it != cfgChildren.end(); ++it)
    {
        if (!(*it)->isVisible())
            continue;

        child = (*it)->configWidget(cfgGrp, parent);
        wizard->addPage(child, (*it)->getLabel());
    }

    if (child)
        wizard->setFinishEnabled(child, true);
        
    return wizard;
}

JumpPane::JumpPane(const QStringList &labels, const QStringList &helptext) :
    VerticalConfigurationGroup(true, false, true, true)
{
    //setLabel(tr("Jump To Buttons"));
    for (uint i = 0; i < labels.size(); i++)
    {
        TransButtonSetting *button =
            new TransButtonSetting(QString::number(i));
        button->setLabel(labels[i]);
        button->setHelpText(helptext[i]);
        connect(button, SIGNAL(pressed(QString)),
                this,   SIGNAL(pressed(QString)));
        addChild(button);
    }
}

MythDialog *JumpConfigurationWizard::dialogWidget(MythMainWindow *parent,
                                                  const char *widgetName)
{
    MythJumpWizard *wizard = new MythJumpWizard(parent, widgetName);
    dialog = wizard;

    QObject::connect(cfgGrp, SIGNAL(changeHelpText(QString)),
                     wizard, SLOT(  setHelpText(   QString)));

    childWidgets.clear();
    QStringList labels, helptext;
    for (uint i = 0; i < cfgChildren.size(); i++)
    {
        if (cfgChildren[i]->isVisible())
        {
            childWidgets.push_back(cfgChildren[i]->configWidget(cfgGrp, parent));
            labels.push_back(cfgChildren[i]->getLabel());
            helptext.push_back(cfgChildren[i]->getHelpText());
        }
    }

    JumpPane *jumppane = new JumpPane(labels, helptext);
    QWidget  *widget   = jumppane->configWidget(cfgGrp, parent, "JumpCfgWiz");
    wizard->addPage(widget, "");
    wizard->setFinishEnabled(widget, true);
    connect(jumppane, SIGNAL(pressed( QString)),
            this,     SLOT(  showPage(QString)));

    for (uint i = 0; i < childWidgets.size(); i++)
    {
        wizard->addPage(childWidgets[i], labels[i]);
        wizard->setFinishEnabled(childWidgets[i], true);
    }

    return wizard;
}

void JumpConfigurationWizard::showPage(QString page)
{
    uint pagenum = page.toUInt();
    if (pagenum >= childWidgets.size() || !dialog)
        return;
    ((MythJumpWizard*)(dialog))->showPage(childWidgets[pagenum]);
}

void SimpleDBStorage::load() 
{
    MSqlBindings bindings;
    QString querystr = QString("SELECT " + column + " FROM "
           + table + " WHERE " + whereClause(bindings) + ";");
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValues(bindings);
    query.exec();

    if (query.isActive() && query.size() > 0) 
    {
        query.next();
        QString result = query.value(0).toString();
        if (result != QString::null) 
        {
          result = QString::fromUtf8(query.value(0).toString());
          setting->setValue(result);
          setting->setUnchanged();
        }
    }
}

void SimpleDBStorage::save(QString table) 
{
    if (!setting->isChanged())
        return;

    MSqlBindings bindings;
    QString querystr = QString("SELECT * FROM " + table + " WHERE "
            + whereClause(bindings) + ";");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValues(bindings);
    query.exec();

    if (query.isActive() && query.size() > 0) {
        // Row already exists
        // Don"t change this QString. See the CVS logs rev 1.91.
        MSqlBindings bindings;

        querystr = QString("UPDATE " + table + " SET " + setClause(bindings) + 
                           " WHERE " + whereClause(bindings) + ";");

        query.prepare(querystr);
        query.bindValues(bindings);
        query.exec();

        if (!query.isActive())
            MythContext::DBError("simpledbstorage update", querystr);
    } else {
        // Row does not exist yet
        MSqlBindings bindings;

        querystr = QString("INSERT INTO " + table + " SET "
                + setClause(bindings) + ";");

        query.prepare(querystr);
        query.bindValues(bindings);
        query.exec();

        if (!query.isActive())
            MythContext::DBError("simpledbstorage update", querystr);
    }
}

void SimpleDBStorage::save() 
{
    save(table);
}

void AutoIncrementDBSetting::save(QString table)
{
    if (intValue() == 0) 
    {
        // Generate a new, unique ID
        QString querystr = QString("INSERT INTO " + table + " (" 
                + column + ") VALUES (0);");

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec(querystr);

        if (!query.isActive() || query.numRowsAffected() < 1) 
        {
            MythContext::DBError("inserting row", query);
            return;
        }
        setValue(query.lastInsertId().toInt());
    }
}

void AutoIncrementDBSetting::save() 
{
    save(table);
}

void ListBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (widget)
        widget->setEnabled(b);
}

void ListBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    SelectSetting::addSelection(label, value, select);
    if (widget)
        widget->insertItem(label);
};

QWidget* ListBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                      const char* widgetName) {
    QWidget* box = new QVBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel());
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    widget = new MythListBox(box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setHelpText(getHelpText());

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
        if (isSet && current == i)
            widget->setCurrentItem(i);
    }

    connect(widget, SIGNAL(destroyed()),
            this, SLOT(widgetDestroyed()));
    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));
    connect(widget, SIGNAL(accepted(int)),
            this, SIGNAL(accepted(int)));
    connect(widget, SIGNAL(menuButtonPressed(int)),
            this, SIGNAL(menuButtonPressed(int)));
    connect(widget, SIGNAL(editButtonPressed(int)),
            this, SIGNAL(editButtonPressed(int)));
    connect(widget, SIGNAL(deleteButtonPressed(int)),
            this, SIGNAL(deleteButtonPressed(int)));
    connect(this, SIGNAL(valueChanged(const QString&)),
            widget, SLOT(setCurrentItem(const QString&)));
    connect(widget, SIGNAL(highlighted(int)),
            this, SLOT(setValueByIndex(int)));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setFocus();
    widget->setSelectionMode(selectionMode);

    return box;
}

void ListBoxSetting::setSelectionMode(MythListBox::SelectionMode mode)
{
   selectionMode = mode;
   if (widget)
       widget->setSelectionMode(selectionMode);
}

void ListBoxSetting::setValueByIndex(int index)
{
    if (((uint)index) < values.size())
        setValue(values[index]);
}

void ImageSelectSetting::addImageSelection(const QString& label,
                                           QImage* image,
                                           QString value,
                                           bool select) {
    images.push_back(image);
    addSelection(label, value, select);
}

ImageSelectSetting::~ImageSelectSetting() {
    while (images.size() > 0) {
        delete images.back();
        images.pop_back();
    }
}

void ImageSelectSetting::imageSet(int num)
{
    if (num >= (int)images.size())
        return;

    if (!images[current])
        return;

    QImage temp = *(images[current]);
    temp = temp.smoothScale((int)(184 * m_hmult), (int)(138 * m_hmult),
                            QImage::ScaleMin);

    QPixmap tmppix(temp);
    imagelabel->setPixmap(tmppix);
}

QWidget* ImageSelectSetting::configWidget(ConfigurationGroup *cg, 
                                          QWidget* parent,
                                          const char* widgetName) 
{
    int width = 0, height = 0;

    gContext->GetScreenSettings(width, m_wmult, height, m_hmult);

    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    MythComboBox *widget = new MythComboBox(false, box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel *testlabel = new QLabel(box);
    testlabel->setText("  ");
    testlabel->setBackgroundOrigin(QWidget::WindowOrigin);

    imagelabel = new QLabel(box);
    imagelabel->setBackgroundOrigin(QWidget::WindowOrigin);

    for (unsigned int i = 0 ; i < images.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);
    else
        current = 0;

    if (images.size() != 0 && current < images.size() && images[current])
    { 
        QImage temp = *(images[current]);
        temp = temp.smoothScale((int)(184 * m_hmult), (int)(138 * m_hmult), 
                                QImage::ScaleMin);
 
        QPixmap tmppix(temp);
        imagelabel->setPixmap(tmppix);
    }
    else
    {
        QPixmap tmppix((int)(184 * m_hmult), (int)(138 * m_hmult));
        tmppix.fill(black);

        imagelabel->setPixmap(tmppix);
    }

    connect(widget, SIGNAL(highlighted(int)), this, SLOT(setValue(int)));
    connect(widget, SIGNAL(highlighted(int)), this, SLOT(imageSet(int)));

    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg, 
                SIGNAL(changeHelpText(QString)));

    return box;
}

HostnameSetting::HostnameSetting(Storage *storage) : Setting(storage)
{
    setVisible(false);
    
    setValue(gContext->GetHostName());
}

void ChannelSetting::fillSelections(SelectSetting* setting) {

    // this should go somewhere else, in something that knows about
    // channels and how they're stored in the database.  We're just a
    // selector.

    MSqlQuery query(MSqlQuery::InitCon()); 
    query.prepare("SELECT name, chanid FROM channel;");
    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
            setting->addSelection(query.value(0).toString(),
                                  QString::number(query.value(1).toInt()));
}

QWidget* ButtonSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                     const char* widgetName)
{
    (void) cg;
    button = new MythPushButton(parent, widgetName);

    button->setText(getLabel());
    //button->setHelpText(getHelpText());

    connect(button, SIGNAL(pressed()), this, SIGNAL(pressed()));
    connect(button, SIGNAL(pressed()), this, SLOT(SendPressedString()));

    if (cg)
        connect(button, SIGNAL(changeHelpText(QString)),
                cg, SIGNAL(changeHelpText(QString)));

    return button;
}

void ButtonSetting::SendPressedString(void)
{
    emit pressed(name);
}

void ButtonSetting::setEnabled(bool fEnabled)
{
    Configurable::setEnabled(fEnabled);
    if (button)
        button->setEnabled(fEnabled);
}

QWidget* ProgressSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                       const char* widgetName) {
    (void)cg;

    QHBox* widget = new QHBox(parent,widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(getLabel() + "     :", widget, widgetName);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QProgressBar* progress = new QProgressBar(totalSteps, widget, widgetName);
    progress->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(int)), progress, SLOT(setProgress(int)));
    progress->setProgress(intValue());
    return widget;
}

void ConfigPopupDialogWidget::keyPressEvent(QKeyEvent* e) {
    switch(e->key()) {
    case Key_Escape:
        reject();
        emit popupDone();
        break;
    default:
        MythDialog::keyPressEvent(e);
    }
}

MythDialog* ConfigurationPopupDialog::dialogWidget(MythMainWindow* parent,
                                                   const char* widgetName)
{
    dialog = new ConfigPopupDialogWidget(parent, widgetName);
    dialog->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "") {
        QHBox* box = new QHBox(dialog);
        box->setBackgroundOrigin(QWidget::WindowOrigin);
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));

        QLabel* label = new QLabel(box);
        label->setText(getLabel());
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        label->setAlignment(Qt::AlignHCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                         QSizePolicy::Maximum));

        dialog->addWidget(box);
    }

    QWidget *widget = configWidget(NULL, dialog, "ConfigurationPopup");
    dialog->addWidget(widget);
    widget->setFocus();

    return dialog;
}

int ConfigurationPopupDialog::exec(bool saveOnAccept)
{
    storage->load();

    dialog = (ConfigPopupDialogWidget*)
        dialogWidget(gContext->GetMainWindow(), "ConfigurationPopupDialog");
    dialog->ShowPopup(this);

    int ret = dialog->exec();

    if ((QDialog::Accepted == ret) && saveOnAccept)
        storage->save();

    return ret;
}

HostDBStorage::HostDBStorage(Setting *_setting, QString name) :
    SimpleDBStorage(_setting, "settings", "data")
{
    _setting->setName(name);
}

GlobalDBStorage::GlobalDBStorage(Setting *_setting, QString name) :
    SimpleDBStorage(_setting, "settings", "data")
{
    _setting->setName(name);
}

QString SimpleDBStorage::setClause(MSqlBindings &bindings)
{
    QString tagname(":SET" + column.upper());
    QString clause(column + " = " + tagname);

    bindings.insert(tagname, setting->getValue().utf8());

    return clause;
}

QString HostDBStorage::whereClause(MSqlBindings &bindings)
{
    /* Returns a where clause of the form:
     * "value = :VALUE AND hostname = :HOSTNAME"
     * The necessary bindings are added to the MSQLBindings&
     */
    QString valueTag(":WHEREVALUE");
    QString hostnameTag(":WHEREHOSTNAME");

    QString clause("value = " + valueTag + " AND hostname = " + hostnameTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(hostnameTag, gContext->GetHostName());

    return clause;
}

QString HostDBStorage::setClause(MSqlBindings &bindings)
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");
    QString hostnameTag(":SETHOSTNAME");
    QString clause("value = " + valueTag + ", data = " + dataTag
            + ", hostname = " + hostnameTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(dataTag, setting->getValue().utf8());
    bindings.insert(hostnameTag, gContext->GetHostName());

    return clause;
}

QString GlobalDBStorage::whereClause(MSqlBindings &bindings)
{
    QString valueTag(":WHEREVALUE");
    QString clause("value = " + valueTag);

    bindings.insert(valueTag, setting->getName());

    return clause;
}

QString GlobalDBStorage::setClause(MSqlBindings &bindings)
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");

    QString clause("value = " + valueTag + ", data = " + dataTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(dataTag, setting->getValue().utf8());

    return clause;
}
