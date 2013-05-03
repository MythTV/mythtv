// C++ headers
#include <algorithm>
#include <cmath>
using namespace std;

// POSIX headers
#include <unistd.h>

// Qt widgets
#include <QLineEdit>
#include <QLabel>
#include <QSlider>
#include <QLCDNumber>
#include <QButtonGroup>
#include <QRadioButton>
#include <QProgressBar>

// Qt utils
#include <QFile>
#include <QDateTime>
#include <QDir>

// MythTV headers
#define MYTHCONFIG
#include "settings.h"
#include "mythconfiggroups.h"
#undef MYTHCONFIG

#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "DisplayRes.h"
#include "mythuihelper.h"

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
    LOG(VB_GENERAL, LOG_ALERT,
             "BUG: Configurable is visible, but has no configWidget");
    return NULL;
}

/** \brief This slot calls the virtual widgetInvalid(QObject*) method.
 *
 *  This should not be needed, anyone calling configWidget() should
 *  also be calling widgetInvalid() directly before configWidget()
 *  is called again on the Configurable. If widgetInvalid() is not
 *  called directly before the Configurable's configWidget() is
 *  called the Configurable may not update properly on screen, but
 *  if this is connected to from the widget's destroyed(QObject*)
 *  signal this will prevent a segfault from occurring.
 */
void Configurable::widgetDeleted(QObject *obj)
{
    widgetInvalid(obj);
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
    return settingValue;
}

void Setting::setValue(const QString &newValue)
{
    settingValue = newValue;
    emit valueChanged(settingValue);
}

int SelectSetting::findSelection(const QString &label, QString value) const
{
    value = (value.isEmpty()) ? label : value;

    for (uint i = 0; i < values.size(); i++)
    {
        if ((values[i] == value) && (labels[i] == label))
            return i;
    }

    return -1;
}

void SelectSetting::addSelection(const QString &label, QString value,
                                 bool select)
{
    value = (value.isEmpty()) ? label : value;

    int found = findSelection(label, value);
    if (found < 0)
    {
        labels.push_back(label);
        values.push_back(value);
        emit selectionAdded( label, value);
    }

    if (select || !isSet)
        setValue(value);
}

bool SelectSetting::removeSelection(const QString &label, QString value)
{
    value = (value.isEmpty()) ? label : value;

    int found = findSelection(label, value);
    if (found < 0)
        return false;

    bool wasSet = isSet;
    isSet = false;

    labels.erase(labels.begin() + found);
    values.erase(values.begin() + found);

    isSet = wasSet && labels.size();
    if (isSet)
    {
        current = (current > (uint)found) ? current - 1 : current;
        current = min(current, (uint) (labels.size() - 1));
    }

    emit selectionRemoved(label, value);

    return true;
}

void SelectSetting::fillSelectionsFromDir(const QDir& dir, bool absPath)
{
    QFileInfoList il = dir.entryInfoList();

    for (QFileInfoList::Iterator it = il.begin();
                                 it != il.end();
                               ++it )
    {
        QFileInfo &fi = *it;

        if (absPath)
            addSelection( fi.absoluteFilePath() );
        else
            addSelection( fi.fileName() );
    }
}

void SelectSetting::clearSelections(void) {
    labels.clear();
    values.clear();
    isSet = false;
    emit selectionsCleared();
}

void SelectSetting::setValue(const QString &newValue)
{
    int found = getValueIndex(newValue);
    if (found < 0)
    {
        addSelection(newValue, newValue, true);
    }
    else
    {
        current = found;
        isSet   = true;
        Setting::setValue(newValue);
    }
}

void SelectSetting::setValue(int which)
{
    if ((which >= ((int) values.size())) || (which < 0))
    {
        LOG(VB_GENERAL, LOG_ERR,
                 QString("SelectSetting::setValue(): invalid index: %1 size: ")
                     .arg(which).arg(values.size()));
    }
    else
    {
        current = which;
        isSet   = true;
        Setting::setValue(values[current]);
    }
}

QString SelectSetting::getSelectionLabel(void) const
{
    if (!isSet || (current >= values.size()))
        return QString::null;

    return labels[current];
}

/** \fn SelectSetting::getValueIndex(QString)
 *  \brief Returns index of value in SelectSetting, or -1 if not found.
 */
int SelectSetting::getValueIndex(QString value)
{
    int ret = 0;

    selectionList::const_iterator it = values.begin();
    for (; it != values.end(); ++it, ++ret)
    {
        if (*it == value)
            return ret;
    }

    return -1;
}

bool SelectSetting::ReplaceLabel(const QString &new_label, const QString &value)
{
    int i = getValueIndex(value);

    if (i >= 0)
        labels[i] = new_label;

    return (i >= 0);
}

QWidget* LabelSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    (void)cg;

    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    QLabel *value = new QLabel();
    value->setText(getValue());
    layout->addWidget(value);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    widget->setLayout(layout);

    return widget;
}

QWidget* LineEditSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char *widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = NULL;
    if (labelAboveWidget)
    {
        layout = new QVBoxLayout();
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                                          QSizePolicy::Maximum));
    }
    else
        layout = new QHBoxLayout();

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    bxwidget = widget;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    edit = new MythLineEdit(
        settingValue, NULL,
        QString(QString(widgetName) + "-edit").toLatin1().constData());
    edit->setHelpText(getHelpText());
    edit->setText( getValue() );
    edit->setMinimumHeight(25);
    layout->addWidget(edit);

    connect(this, SIGNAL(valueChanged(const QString&)),
            edit, SLOT(setText(const QString&)));
    connect(edit, SIGNAL(textChanged(const QString&)),
            this, SLOT(setValue(const QString&)));

    if (cg)
        connect(edit, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    setRW(rw);
    SetPasswordEcho(password_echo);

    widget->setLayout(layout);

    return widget;
}

void LineEditSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        edit     = NULL;
    }
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

void LineEditSetting::SetPasswordEcho(bool b)
{
    password_echo = b;
    if (edit)
        edit->setEchoMode(b ? QLineEdit::Password : QLineEdit::Normal);
}

void LineEditSetting::setHelpText(const QString &str)
{
    if (edit)
        edit->setHelpText(str);
    Setting::setHelpText(str);
}

void BoundedIntegerSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, max), min);
    IntegerSetting::setValue(newValue);
}

QWidget* SliderSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                     const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = NULL;
    if (labelAboveWidget)
    {
        layout = new QVBoxLayout();
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                                          QSizePolicy::Maximum));
    }
    else
        layout = new QHBoxLayout();

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setObjectName(QString(widgetName) + "-label");
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    MythSlider *slider = new MythSlider(
        NULL, QString(QString(widgetName) + "-slider").toLatin1().constData());
    slider->setHelpText(getHelpText());
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setOrientation( Qt::Horizontal );
    slider->setSingleStep(step);
    slider->setValue(intValue());
    layout->addWidget(slider);

    QLCDNumber *lcd = new QLCDNumber();
    lcd->setObjectName(QString(QString(widgetName) + "-lcd")
                       .toLatin1().constData());
    lcd->setMode(QLCDNumber::Dec);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->display(intValue());
    layout->addWidget(lcd);

    connect(slider, SIGNAL(valueChanged(int)), lcd, SLOT(display(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

    if (cg)
        connect(slider, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setLayout(layout);

    return widget;
}

SpinBoxSetting::SpinBoxSetting(
    Storage *_storage, int _min, int _max, int _step,
    bool _allow_single_step, QString _special_value_text) :
    BoundedIntegerSetting(_storage, _min, _max, _step),
    spinbox(NULL), relayEnabled(true),
    sstep(_allow_single_step), svtext("")
{
    if (!_special_value_text.isEmpty())
        svtext = _special_value_text;

    IntegerSetting *iset = (IntegerSetting *) this;
    connect(iset, SIGNAL(valueChanged(     int)),
            this, SLOT(  relayValueChanged(int)));
}

QWidget* SpinBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = NULL;
    if (labelAboveWidget)
    {
        layout = new QVBoxLayout();
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                                          QSizePolicy::Maximum));
    }
    else
        layout = new QHBoxLayout();

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    bxwidget = widget;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    QString sbname = QString(widgetName) + "MythSpinBox";
    spinbox = new MythSpinBox(NULL, sbname.toLatin1().constData(), sstep);
    spinbox->setHelpText(getHelpText());
    spinbox->setMinimum(min);
    spinbox->setMaximum(max);
    spinbox->setMinimumHeight(25);
    layout->addWidget(spinbox);

    // only set step size if greater than default (1), otherwise
    // this will screw up the single-step/jump behavior of the MythSpinBox
    if (1 < step)
        spinbox->setSingleStep(step);
    spinbox->setValue(intValue());
    if (!svtext.isEmpty())
        spinbox->setSpecialValueText(svtext);

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));

    if (cg)
        connect(spinbox, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setLayout(layout);

    return widget;
}

void SpinBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        spinbox  = NULL;
    }
}

void SpinBoxSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, max), min);
    if (spinbox && (spinbox->value() != newValue))
    {
        //int old = intValue();
        spinbox->setValue(newValue);
    }
    else if (intValue() != newValue)
    {
        BoundedIntegerSetting::setValue(newValue);
    }
}

void SpinBoxSetting::setFocus(void)
{
    if (spinbox)
        spinbox->setFocus();
}

void SpinBoxSetting::clearFocus(void)
{
    if (spinbox)
        spinbox->clearFocus();
}

bool SpinBoxSetting::hasFocus(void) const
{
    if (spinbox)
        return spinbox->hasFocus();

    return false;
}

void SpinBoxSetting::relayValueChanged(int newValue)
{
    if (relayEnabled)
        emit valueChanged(configName, newValue);
}

void SpinBoxSetting::setHelpText(const QString &str)
{
    if (spinbox)
        spinbox->setHelpText(str);
    BoundedIntegerSetting::setHelpText(str);
}

QWidget* SelectLabelSetting::configWidget(ConfigurationGroup *cg,
                                          QWidget    *parent,
                                          const char *widgetName)
{
    (void)cg;

    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = NULL;
    if (labelAboveWidget)
    {
        layout = new QVBoxLayout();
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                                          QSizePolicy::Maximum));
    }
    else
        layout = new QHBoxLayout();

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }


    QLabel *value = new QLabel();
    value->setText(labels[current]);
    layout->addWidget(value);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    widget->setLayout(layout);

    return widget;
}

QWidget* ComboBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = NULL;
    if (labelAboveWidget)
    {
        layout = new QVBoxLayout();
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                                          QSizePolicy::Maximum));
    }
    else
        layout = new QHBoxLayout();

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    bxwidget = widget;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    cbwidget = new MythComboBox(rw);
    cbwidget->setHelpText(getHelpText());

    for(unsigned int i = 0 ; i < labels.size() ; ++i)
        cbwidget->insertItem(labels[i]);

    resetMaxCount(cbwidget->count());

    if (isSet)
        cbwidget->setCurrentIndex(current);

    if (1 < step)
        cbwidget->setStep(step);

    connect(cbwidget, SIGNAL(highlighted(int)),
            this, SLOT(setValue(int)));
    connect(cbwidget, SIGNAL(activated(int)),
            this, SLOT(setValue(int)));
    connect(this, SIGNAL(selectionsCleared()),
            cbwidget, SLOT(clear()));

    if (rw)
        connect(cbwidget, SIGNAL(editTextChanged(const QString &)),
                this, SLOT(editTextChanged(const QString &)));

    if (cg)
        connect(cbwidget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    cbwidget->setMinimumHeight(25);

    layout->addWidget(cbwidget);
    layout->setStretchFactor(cbwidget, 1);

    widget->setLayout(layout);

    return widget;
}

void ComboBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        cbwidget = NULL;
    }
}

void ComboBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (cbwidget)
        cbwidget->setEnabled(b);
}

void ComboBoxSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (cbwidget)
    {
        if (b)
            cbwidget->show();
        else
            cbwidget->hide();
    }
}

void ComboBoxSetting::setValue(const QString &newValue)
{
    for (uint i = 0; i < values.size(); i++)
    {
        if (values[i] == newValue)
        {
            setValue(i);
            break;
        }
    }

    if (rw)
    {
        Setting::setValue(newValue);
        if (cbwidget)
            cbwidget->setCurrentIndex(current);
    }
}

void ComboBoxSetting::setValue(int which)
{
    if (cbwidget)
        cbwidget->setCurrentIndex(which);
    SelectSetting::setValue(which);
}

void ComboBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    if ((findSelection(label, value) < 0) && cbwidget)
    {
        resetMaxCount(cbwidget->count()+1);
        cbwidget->insertItem(label);
    }

    SelectSetting::addSelection(label, value, select);

    if (cbwidget && isSet)
        cbwidget->setCurrentIndex(current);
}

bool ComboBoxSetting::removeSelection(const QString &label, QString value)
{
    SelectSetting::removeSelection(label, value);
    if (!cbwidget)
        return true;

    for (uint i = 0; ((int) i) < cbwidget->count(); i++)
    {
        if (cbwidget->itemText(i) == label)
        {
            cbwidget->removeItem(i);
            if (isSet)
                cbwidget->setCurrentIndex(current);
            resetMaxCount(cbwidget->count());
            return true;
        }
    }

    return false;
}

void ComboBoxSetting::editTextChanged(const QString &newText)
{
    if (cbwidget)
    {
        for (uint i = 0; i < labels.size(); i++)
            if (labels[i] == newText)
                return;

        if (labels.size() == static_cast<size_t>(cbwidget->maxCount()))
        {
            SelectSetting::removeSelection(labels[cbwidget->maxCount() - 1],
                                           values[cbwidget->maxCount() - 1]);
            cbwidget->setItemText(cbwidget->maxCount() - 1, newText);
        }
        else
        {
            cbwidget->insertItem(newText);
        }

        SelectSetting::addSelection(newText, newText, true);
        cbwidget->setCurrentIndex(cbwidget->maxCount() - 1);
    }
}

void ComboBoxSetting::setHelpText(const QString &str)
{
    if (cbwidget)
        cbwidget->setHelpText(str);
    SelectSetting::setHelpText(str);
}

void HostRefreshRateComboBox::ChangeResolution(const QString& resolution)
{
    clearSelections();

    const vector<double> list = GetRefreshRates(resolution);
    addSelection(QObject::tr("Auto"), "0");
    int hz50 = -1, hz60 = -1;
    for (uint i=0; i<list.size(); ++i)
    {
        QString sel = QString::number((double) list[i],'f', 3);
        addSelection(sel+" Hz", sel);
        hz50 = (fabs(50.0 - list[i]) < 0.01) ? i : hz50;
        hz60 = (fabs(60.0 - list[i]) < 0.01) ? i : hz60;
    }

    setValue(0);
    if ("640x480" == resolution || "720x480" == resolution)
        setValue(hz60+1);
    if ("640x576" == resolution || "720x576" == resolution)
        setValue(hz50+1);

    setEnabled(list.size());
}

const vector<double> HostRefreshRateComboBox::GetRefreshRates(const QString &res)
{
    QStringList slist = res.split("x");
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

    vector<double> list;
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

QString DateSetting::getValue(void) const
{
    return settingValue;
}

QDate DateSetting::dateValue(void) const {
    return QDate::fromString(getValue(), Qt::ISODate);
}

void DateSetting::setValue(const QDate& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

void DateSetting::setValue(const QString &newValue)
{
    QDate date = QDate::fromString(newValue, Qt::ISODate);
    if (date.isValid())
        setValue(date);
}

void TimeSetting::setValue(const QString &newValue)
{
    QTime time = QTime::fromString(newValue, Qt::ISODate);
    if (time.isValid())
        setValue(time);
}

QWidget* CheckBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName) {
    widget = new MythCheckBox(parent, widgetName);
    connect(widget, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    widget->setHelpText(getHelpText());
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

void CheckBoxSetting::widgetInvalid(QObject *obj)
{
    widget = (widget == obj) ? NULL : widget;
}

void CheckBoxSetting::setVisible(bool b)
{
    BooleanSetting::setVisible(b);
    if (widget)
    {
        if (b)
            widget->show();
        else
            widget->hide();
    }
}

void CheckBoxSetting::setLabel(QString str)
{
    BooleanSetting::setLabel(str);
    if (widget)
        widget->setText(str);
}

void CheckBoxSetting::setEnabled(bool fEnabled)
{
    BooleanSetting::setEnabled(fEnabled);
    if (widget)
        widget->setEnabled(fEnabled);
}

void CheckBoxSetting::setHelpText(const QString &str)
{
    if (widget)
        widget->setHelpText(str);
    BooleanSetting::setHelpText(str);
}

void AutoIncrementDBSetting::Save(QString table)
{
    if (intValue() == 0)
    {
        // Generate a new, unique ID
        QString querystr = QString("INSERT INTO " + table + " ("
                + GetColumnName() + ") VALUES (0);");

        MSqlQuery query(MSqlQuery::InitCon());

        if (!query.exec(querystr))
        {
            MythDB::DBError("inserting row", query);
            return;
        }
        // XXX -- HACK BEGIN:
        // lastInsertID fails with "QSqlQuery::value: not positioned on a valid record"
        // if we get a invalid QVariant we workaround the problem by taking advantage
        // of mysql always incrementing the auto increment pointer
        // this breaks if someone modifies the auto increment pointer
        //setValue(query.lastInsertId().toInt());

        QVariant var = query.lastInsertId();

        if (var.type())
            setValue(var.toInt());
        else
        {
            querystr = QString("SELECT MAX(" + GetColumnName() + ") FROM " + table + ";");
            if (query.exec(querystr) && query.next())
            {
                int lii = query.value(0).toInt();
                lii = lii ? lii : 1;
                setValue(lii);
            }
            else
                LOG(VB_GENERAL, LOG_EMERG,
                         "Can't determine the Id of the last insert "
                         "QSqlQuery.lastInsertId() failed, the workaround "
                         "failed too!");
        }
        // XXX -- HACK END:
    }
}

void AutoIncrementDBSetting::Save(void)
{
    Save(GetTableName());
}

void ListBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (lbwidget)
        lbwidget->setEnabled(b);
}

void ListBoxSetting::clearSelections(void)
{
    SelectSetting::clearSelections();
    if (lbwidget)
        lbwidget->clear();
}

void ListBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    SelectSetting::addSelection(label, value, select);
    if (lbwidget)
    {
        lbwidget->insertItem(label);
        //lbwidget->triggerUpdate(true);
    }
};

bool ListBoxSetting::ReplaceLabel(
    const QString &new_label, const QString &value)
{
    int i = getValueIndex(value);

    if ((i >= 0) && SelectSetting::ReplaceLabel(label, value) && lbwidget)
    {
        lbwidget->changeItem(new_label, i);
        return true;
    }

    return false;
}

QWidget* ListBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QVBoxLayout *layout = new QVBoxLayout();

    if (getLabel() != "")
    {
        QLabel *label = new QLabel();
        label->setText(getLabel());
        layout->addWidget(label);
    }

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    bxwidget = widget;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    lbwidget = new MythListBox(NULL);
    lbwidget->setHelpText(getHelpText());
    if (eventFilter)
        lbwidget->installEventFilter(eventFilter);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        lbwidget->insertItem(labels[i]);
        if (isSet && current == i)
            lbwidget->setCurrentRow(i);
    }

    connect(this,     SIGNAL(selectionsCleared()),
            lbwidget, SLOT(  clear()));
    connect(this,     SIGNAL(valueChanged(const QString&)),
            lbwidget, SLOT(  setCurrentItem(const QString&)));

    connect(lbwidget, SIGNAL(accepted(int)),
            this,     SIGNAL(accepted(int)));
    connect(lbwidget, SIGNAL(menuButtonPressed(int)),
            this,     SIGNAL(menuButtonPressed(int)));
    connect(lbwidget, SIGNAL(editButtonPressed(int)),
            this,     SIGNAL(editButtonPressed(int)));
    connect(lbwidget, SIGNAL(deleteButtonPressed(int)),
            this,     SIGNAL(deleteButtonPressed(int)));

    connect(lbwidget, SIGNAL(highlighted(int)),
            this,     SLOT(  setValueByIndex(int)));

    if (cg)
        connect(lbwidget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    lbwidget->setFocus();
    lbwidget->setSelectionMode(selectionMode);
    layout->addWidget(lbwidget);

    widget->setLayout(layout);

    return widget;
}

void ListBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        lbwidget = NULL;
    }
}

void ListBoxSetting::setSelectionMode(MythListBox::SelectionMode mode)
{
   selectionMode = mode;
   if (lbwidget)
       lbwidget->setSelectionMode(selectionMode);
}

void ListBoxSetting::setValueByIndex(int index)
{
    if (((uint)index) < values.size())
        setValue(values[index]);
}

void ListBoxSetting::setHelpText(const QString &str)
{
    if (lbwidget)
        lbwidget->setHelpText(str);
    SelectSetting::setHelpText(str);
}

HostnameSetting::HostnameSetting(Storage *storage) : Setting(storage)
{
    setVisible(false);

    setValue(gCoreContext->GetHostName());
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
    connect(button, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    button->setText(getLabel());
    button->setHelpText(getHelpText());

    connect(button, SIGNAL(pressed()), this, SIGNAL(pressed()));
    connect(button, SIGNAL(pressed()), this, SLOT(SendPressedString()));

    if (cg)
        connect(button, SIGNAL(changeHelpText(QString)),
                cg, SIGNAL(changeHelpText(QString)));

    return button;
}

void ButtonSetting::widgetInvalid(QObject *obj)
{
    button = (button == obj) ? NULL : button;
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

void ButtonSetting::setLabel(QString str)
{
    if (button)
        button->setText(str);
    Setting::setLabel(str);
}

void ButtonSetting::setHelpText(const QString &str)
{
    if (button)
        button->setHelpText(str);
    Setting::setHelpText(str);
}

QWidget* ProgressSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                       const char* widgetName) {
    (void)cg;
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);
    QBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel();
        label->setObjectName(QString(widgetName) + "_label");
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    QProgressBar *progress = new QProgressBar(NULL);
    progress->setObjectName(widgetName);
    progress->setRange(0,totalSteps);
    layout->addWidget(progress);

    connect(this, SIGNAL(valueChanged(int)), progress, SLOT(setValue(int)));
    progress->setValue(intValue());

    widget->setLayout(layout);

    return widget;
}
