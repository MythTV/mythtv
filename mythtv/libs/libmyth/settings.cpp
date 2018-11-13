// C++ headers
#include <algorithm>
#include <cmath>
using namespace std;

// POSIX headers
#include <unistd.h>

// Qt widgets
#include <QLineEdit>
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
 *   and return a nullptr.
 */

QWidget* Configurable::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName)
{
    (void)cg;
    (void)parent;
    (void)widgetName;
    LOG(VB_GENERAL, LOG_ALERT,
             "BUG: Configurable is visible, but has no configWidget");
    return nullptr;
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
    return m_settingValue;
}

void Setting::setValue(const QString &newValue)
{
    m_settingValue = newValue;
    emit valueChanged(m_settingValue);
}

int SelectSetting::findSelection(const QString &label, QString value) const
{
    value = (value.isEmpty()) ? label : value;

    for (uint i = 0; i < m_values.size(); i++)
    {
        if ((m_values[i] == value) && (m_labels[i] == label))
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
        m_labels.push_back(label);
        m_values.push_back(value);
        emit selectionAdded( label, value);
    }

    if (select || !m_isSet)
        setValue(value);
}

bool SelectSetting::removeSelection(const QString &label, QString value)
{
    value = (value.isEmpty()) ? label : value;

    int found = findSelection(label, value);
    if (found < 0)
        return false;

    bool wasSet = m_isSet;
    m_isSet = false;

    m_labels.erase(m_labels.begin() + found);
    m_values.erase(m_values.begin() + found);

    m_isSet = wasSet && m_labels.size();
    if (m_isSet)
    {
        m_current = (m_current > (uint)found) ? m_current - 1 : m_current;
        m_current = min(m_current, (uint) (m_labels.size() - 1));
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
    m_labels.clear();
    m_values.clear();
    m_isSet = false;
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
        m_current = found;
        m_isSet   = true;
        Setting::setValue(newValue);
    }
}

void SelectSetting::setValue(int which)
{
    if ((which >= ((int) m_values.size())) || (which < 0))
    {
        LOG(VB_GENERAL, LOG_ERR,
                 QString("SelectSetting::setValue(): invalid index: %1 size: %2")
                     .arg(which).arg(m_values.size()));
    }
    else
    {
        m_current = which;
        m_isSet   = true;
        Setting::setValue(m_values[m_current]);
    }
}

QString SelectSetting::getSelectionLabel(void) const
{
    if (!m_isSet || (m_current >= m_values.size()))
        return QString();

    return m_labels[m_current];
}

/** \fn SelectSetting::getValueIndex(QString)
 *  \brief Returns index of value in SelectSetting, or -1 if not found.
 */
int SelectSetting::getValueIndex(QString value)
{
    int ret = 0;

    selectionList::const_iterator it = m_values.begin();
    for (; it != m_values.end(); ++it, ++ret)
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
        m_labels[i] = new_label;

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
        MythLabel *label = new MythLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    MythLabel *value = new MythLabel();
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

    QBoxLayout *layout = nullptr;
    if (m_labelAboveWidget)
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
        MythLabel *label = new MythLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    m_bxwidget = widget;
    connect(m_bxwidget, SIGNAL(destroyed(QObject*)),
            this,       SLOT(widgetDeleted(QObject*)));

    m_edit = new MythLineEdit(
        m_settingValue, nullptr,
        QString(QString(widgetName) + "-edit").toLatin1().constData());
    m_edit->setHelpText(getHelpText());
    m_edit->setText( getValue() );
    m_edit->setMinimumHeight(25);
    layout->addWidget(m_edit);

    connect(this,   SIGNAL(valueChanged(const QString&)),
            m_edit, SLOT(setText(const QString&)));
    connect(m_edit, SIGNAL(textChanged(const QString&)),
            this,   SLOT(setValue(const QString&)));

    if (cg)
        connect(m_edit, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    setRW(m_rw);
    SetPasswordEcho(m_password_echo);

    widget->setLayout(layout);

    return widget;
}

void LineEditSetting::widgetInvalid(QObject *obj)
{
    if (m_bxwidget == obj)
    {
        m_bxwidget = nullptr;
        m_edit     = nullptr;
    }
}

void LineEditSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (m_edit)
        m_edit->setEnabled(b);
}

void LineEditSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (m_edit)
    {
        //QWidget *parent = m_edit->parentWidget();
        if (b)
            m_edit->show();
        else
            m_edit->hide();
    }
}

void LineEditSetting::SetPasswordEcho(bool b)
{
    m_password_echo = b;
    if (m_edit)
        m_edit->setEchoMode(b ? QLineEdit::Password : QLineEdit::Normal);
}

void LineEditSetting::setHelpText(const QString &str)
{
    if (m_edit)
        m_edit->setHelpText(str);
    Setting::setHelpText(str);
}

void BoundedIntegerSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, m_max), m_min);
    IntegerSetting::setValue(newValue);
}

QWidget* SliderSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                     const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = nullptr;
    if (m_labelAboveWidget)
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
        MythLabel *label = new MythLabel();
        label->setObjectName(QString(widgetName) + "-label");
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    MythSlider *slider = new MythSlider(
        nullptr, QString(QString(widgetName) + "-slider").toLatin1().constData());
    slider->setHelpText(getHelpText());
    slider->setMinimum(m_min);
    slider->setMaximum(m_max);
    slider->setOrientation( Qt::Horizontal );
    slider->setSingleStep(m_step);
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
    m_bxwidget(nullptr), m_spinbox(nullptr), m_relayEnabled(true),
    m_sstep(_allow_single_step), m_svtext("")
{
    if (!_special_value_text.isEmpty())
        m_svtext = _special_value_text;

    IntegerSetting *iset = (IntegerSetting *) this;
    connect(iset, SIGNAL(valueChanged(     int)),
            this, SLOT(  relayValueChanged(int)));
}

QWidget* SpinBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName)
{
    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = nullptr;
    if (m_labelAboveWidget)
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
        MythLabel *label = new MythLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    m_bxwidget = widget;
    connect(m_bxwidget, SIGNAL(destroyed(QObject*)),
            this,       SLOT(widgetDeleted(QObject*)));

    QString sbname = QString(widgetName) + "MythSpinBox";
    m_spinbox = new MythSpinBox(nullptr, sbname.toLatin1().constData(), m_sstep);
    m_spinbox->setHelpText(getHelpText());
    m_spinbox->setMinimum(m_min);
    m_spinbox->setMaximum(m_max);
    m_spinbox->setMinimumHeight(25);
    layout->addWidget(m_spinbox);

    // only set step size if greater than default (1), otherwise
    // this will screw up the single-step/jump behavior of the MythSpinBox
    if (1 < m_step)
        m_spinbox->setSingleStep(m_step);
    m_spinbox->setValue(intValue());
    if (!m_svtext.isEmpty())
        m_spinbox->setSpecialValueText(m_svtext);

    connect(m_spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));

    if (cg)
        connect(m_spinbox, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setLayout(layout);

    return widget;
}

void SpinBoxSetting::widgetInvalid(QObject *obj)
{
    if (m_bxwidget == obj)
    {
        m_bxwidget = nullptr;
        m_spinbox  = nullptr;
    }
}

void SpinBoxSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, m_max), m_min);
    if (m_spinbox && (m_spinbox->value() != newValue))
    {
        //int old = intValue();
        m_spinbox->setValue(newValue);
    }
    else if (intValue() != newValue)
    {
        BoundedIntegerSetting::setValue(newValue);
    }
}

void SpinBoxSetting::setFocus(void)
{
    if (m_spinbox)
        m_spinbox->setFocus();
}

void SpinBoxSetting::clearFocus(void)
{
    if (m_spinbox)
        m_spinbox->clearFocus();
}

bool SpinBoxSetting::hasFocus(void) const
{
    if (m_spinbox)
        return m_spinbox->hasFocus();

    return false;
}

void SpinBoxSetting::relayValueChanged(int newValue)
{
    if (m_relayEnabled)
        emit valueChanged(m_configName, newValue);
}

void SpinBoxSetting::setHelpText(const QString &str)
{
    if (m_spinbox)
        m_spinbox->setHelpText(str);
    BoundedIntegerSetting::setHelpText(str);
}

QWidget* SelectLabelSetting::configWidget(ConfigurationGroup *cg,
                                          QWidget    *parent,
                                          const char *widgetName)
{
    (void)cg;

    QWidget *widget = new QWidget(parent);
    widget->setObjectName(widgetName);

    QBoxLayout *layout = nullptr;
    if (m_labelAboveWidget)
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
        MythLabel *label = new MythLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }


    MythLabel *value = new MythLabel();
    value->setText(m_labels[m_current]);
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

    QBoxLayout *layout = nullptr;
    if (m_labelAboveWidget)
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
        MythLabel *label = new MythLabel();
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    m_bxwidget = widget;
    connect(m_bxwidget, SIGNAL(destroyed(QObject*)),
            this,       SLOT(widgetDeleted(QObject*)));

    m_cbwidget = new MythComboBox(m_rw);
    m_cbwidget->setHelpText(getHelpText());

    for(unsigned int i = 0 ; i < m_labels.size() ; ++i)
        m_cbwidget->insertItem(m_labels[i]);

    resetMaxCount(m_cbwidget->count());

    if (m_isSet)
        m_cbwidget->setCurrentIndex(m_current);

    if (1 < m_step)
        m_cbwidget->setStep(m_step);

    connect(m_cbwidget, SIGNAL(highlighted(int)),
            this, SLOT(setValue(int)));
    connect(m_cbwidget, SIGNAL(activated(int)),
            this, SLOT(setValue(int)));
    connect(this, SIGNAL(selectionsCleared()),
            m_cbwidget, SLOT(clear()));

    if (m_rw)
        connect(m_cbwidget, SIGNAL(editTextChanged(const QString &)),
                this, SLOT(editTextChanged(const QString &)));

    if (cg)
        connect(m_cbwidget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    m_cbwidget->setMinimumHeight(25);

    layout->addWidget(m_cbwidget);
    layout->setStretchFactor(m_cbwidget, 1);

    widget->setLayout(layout);

    return widget;
}

void ComboBoxSetting::widgetInvalid(QObject *obj)
{
    if (m_bxwidget == obj)
    {
        m_bxwidget = nullptr;
        m_cbwidget = nullptr;
    }
}

void ComboBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (m_cbwidget)
        m_cbwidget->setEnabled(b);
}

void ComboBoxSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (m_cbwidget)
    {
        if (b)
            m_cbwidget->show();
        else
            m_cbwidget->hide();
    }
}

void ComboBoxSetting::setValue(const QString &newValue)
{
    for (uint i = 0; i < m_values.size(); i++)
    {
        if (m_values[i] == newValue)
        {
            setValue(i);
            break;
        }
    }

    if (m_rw)
    {
        // Skip parent (because it might add a selection)
        Setting::setValue(newValue);
        if (m_cbwidget)
            m_cbwidget->setCurrentIndex(m_current);
    }
}

void ComboBoxSetting::setValue(int which)
{
    if (m_cbwidget)
        m_cbwidget->setCurrentIndex(which);
    SelectSetting::setValue(which);
}

void ComboBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    if ((findSelection(label, value) < 0) && m_cbwidget)
    {
        resetMaxCount(m_cbwidget->count()+1);
        m_cbwidget->insertItem(label);
    }

    SelectSetting::addSelection(label, value, select);

    if (m_cbwidget && m_isSet)
        m_cbwidget->setCurrentIndex(m_current);
}

bool ComboBoxSetting::removeSelection(const QString &label, QString value)
{
    SelectSetting::removeSelection(label, value);
    if (!m_cbwidget)
        return true;

    for (uint i = 0; ((int) i) < m_cbwidget->count(); i++)
    {
        if (m_cbwidget->itemText(i) == label)
        {
            m_cbwidget->removeItem(i);
            if (m_isSet)
                m_cbwidget->setCurrentIndex(m_current);
            resetMaxCount(m_cbwidget->count());
            return true;
        }
    }

    return false;
}

void ComboBoxSetting::editTextChanged(const QString &newText)
{
    if (m_cbwidget)
    {
        for (uint i = 0; i < m_labels.size(); i++)
            if (m_labels[i] == newText)
                return;

        if (m_labels.size() == static_cast<size_t>(m_cbwidget->maxCount()))
        {
            SelectSetting::removeSelection(m_labels[m_cbwidget->maxCount() - 1],
                                           m_values[m_cbwidget->maxCount() - 1]);
            m_cbwidget->setItemText(m_cbwidget->maxCount() - 1, newText);
        }
        else
        {
            m_cbwidget->insertItem(newText);
        }

        SelectSetting::addSelection(newText, newText, true);
        m_cbwidget->setCurrentIndex(m_cbwidget->maxCount() - 1);
    }
}

void ComboBoxSetting::setHelpText(const QString &str)
{
    if (m_cbwidget)
        m_cbwidget->setHelpText(str);
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
    if (!value.isEmpty())
        pathname = value;

    if (m_mustexist && !QFile(pathname).exists())
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
    return m_settingValue;
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
    m_cbwidget = new MythCheckBox(parent, widgetName);
    connect(m_cbwidget, SIGNAL(destroyed(QObject*)),
            this,       SLOT(widgetDeleted(QObject*)));

    m_cbwidget->setHelpText(getHelpText());
    m_cbwidget->setText(getLabel());
    m_cbwidget->setChecked(boolValue());

    connect(m_cbwidget, SIGNAL(toggled(bool)),
            this,       SLOT(setValue(bool)));
    connect(this,       SIGNAL(valueChanged(bool)),
            m_cbwidget, SLOT(setChecked(bool)));

    if (cg)
        connect(m_cbwidget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return m_cbwidget;
}

void CheckBoxSetting::widgetInvalid(QObject *obj)
{
    m_cbwidget = (m_cbwidget == obj) ? nullptr : m_cbwidget;
}

void CheckBoxSetting::setVisible(bool b)
{
    BooleanSetting::setVisible(b);
    if (m_cbwidget)
    {
        if (b)
            m_cbwidget->show();
        else
            m_cbwidget->hide();
    }
}

void CheckBoxSetting::setLabel(QString str)
{
    // QT treats a single ampersand as special,
    // we must double up ampersands to display them
    str = str.replace(" & ", " && ");
    BooleanSetting::setLabel(str);
    if (m_cbwidget)
        m_cbwidget->setText(str);
}

void CheckBoxSetting::setEnabled(bool fEnabled)
{
    BooleanSetting::setEnabled(fEnabled);
    if (m_cbwidget)
        m_cbwidget->setEnabled(fEnabled);
}

void CheckBoxSetting::setHelpText(const QString &str)
{
    if (m_cbwidget)
        m_cbwidget->setHelpText(str);
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
    if (m_lbwidget)
        m_lbwidget->setEnabled(b);
}

void ListBoxSetting::clearSelections(void)
{
    SelectSetting::clearSelections();
    if (m_lbwidget)
        m_lbwidget->clear();
}

void ListBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    SelectSetting::addSelection(label, value, select);
    if (m_lbwidget)
    {
        m_lbwidget->insertItem(label);
        //m_lbwidget->triggerUpdate(true);
    }
};

bool ListBoxSetting::ReplaceLabel(
    const QString &new_label, const QString &value)
{
    int i = getValueIndex(value);

    if ((i >= 0) && SelectSetting::ReplaceLabel(m_label, value) && m_lbwidget)
    {
        m_lbwidget->changeItem(new_label, i);
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
        MythLabel *label = new MythLabel();
        label->setText(getLabel());
        layout->addWidget(label);
    }

    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    m_bxwidget = widget;
    connect(m_bxwidget, SIGNAL(destroyed(QObject*)),
            this,       SLOT(widgetDeleted(QObject*)));

    m_lbwidget = new MythListBox(nullptr);
    m_lbwidget->setHelpText(getHelpText());
    if (m_eventFilter)
        m_lbwidget->installEventFilter(m_eventFilter);

    for(unsigned int i = 0 ; i < m_labels.size() ; ++i) {
        m_lbwidget->insertItem(m_labels[i]);
        if (m_isSet && m_current == i)
            m_lbwidget->setCurrentRow(i);
    }

    connect(this,       SIGNAL(selectionsCleared()),
            m_lbwidget, SLOT(  clear()));
    connect(this,       SIGNAL(valueChanged(const QString&)),
            m_lbwidget, SLOT(  setCurrentItem(const QString&)));

    connect(m_lbwidget, SIGNAL(accepted(int)),
            this,       SIGNAL(accepted(int)));
    connect(m_lbwidget, SIGNAL(menuButtonPressed(int)),
            this,       SIGNAL(menuButtonPressed(int)));
    connect(m_lbwidget, SIGNAL(editButtonPressed(int)),
            this,       SIGNAL(editButtonPressed(int)));
    connect(m_lbwidget, SIGNAL(deleteButtonPressed(int)),
            this,       SIGNAL(deleteButtonPressed(int)));

    connect(m_lbwidget, SIGNAL(highlighted(int)),
            this,       SLOT(  setValueByIndex(int)));

    if (cg)
        connect(m_lbwidget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    m_lbwidget->setFocus();
    m_lbwidget->setSelectionMode(m_selectionMode);
    layout->addWidget(m_lbwidget);

    widget->setLayout(layout);

    return widget;
}

void ListBoxSetting::widgetInvalid(QObject *obj)
{
    if (m_bxwidget == obj)
    {
        m_bxwidget = nullptr;
        m_lbwidget = nullptr;
    }
}

void ListBoxSetting::setSelectionMode(MythListBox::SelectionMode mode)
{
   m_selectionMode = mode;
   if (m_lbwidget)
       m_lbwidget->setSelectionMode(m_selectionMode);
}

void ListBoxSetting::setValueByIndex(int index)
{
    if (((uint)index) < m_values.size())
        setValue(m_values[index]);
}

void ListBoxSetting::setHelpText(const QString &str)
{
    if (m_lbwidget)
        m_lbwidget->setHelpText(str);
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
    m_button = new MythPushButton(parent, widgetName);
    connect(m_button, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    m_button->setText(getLabel());
    m_button->setHelpText(getHelpText());

    connect(m_button, SIGNAL(pressed()), this, SIGNAL(pressed()));
    connect(m_button, SIGNAL(pressed()), this, SLOT(SendPressedString()));

    if (cg)
        connect(m_button, SIGNAL(changeHelpText(QString)),
                cg, SIGNAL(changeHelpText(QString)));

    return m_button;
}

void ButtonSetting::widgetInvalid(QObject *obj)
{
    m_button = (m_button == obj) ? nullptr : m_button;
}

void ButtonSetting::SendPressedString(void)
{
    emit pressed(m_name);
}

void ButtonSetting::setEnabled(bool fEnabled)
{
    Configurable::setEnabled(fEnabled);
    if (m_button)
        m_button->setEnabled(fEnabled);
}

void ButtonSetting::setLabel(QString str)
{
    if (m_button)
        m_button->setText(str);
    Setting::setLabel(str);
}

void ButtonSetting::setHelpText(const QString &str)
{
    if (m_button)
        m_button->setHelpText(str);
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
        MythLabel* label = new MythLabel();
        label->setObjectName(QString(widgetName) + "_label");
        label->setText(getLabel() + ":     ");
        layout->addWidget(label);
    }

    QProgressBar *progress = new QProgressBar(nullptr);
    progress->setObjectName(widgetName);
    progress->setRange(0,m_totalSteps);
    layout->addWidget(progress);

    connect(this, SIGNAL(valueChanged(int)), progress, SLOT(setValue(int)));
    progress->setValue(intValue());

    widget->setLayout(layout);

    return widget;
}
