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
#include <qwizard.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qwidgetstack.h>
#include <qdialog.h>
#include <qtabdialog.h>
#include <qfile.h>
#include <qlistview.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qwidget.h>

#include "mythcontext.h"

ConfigurationGroup::~ConfigurationGroup()  {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        delete *i;
}

Setting* ConfigurationGroup::byName(QString name) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i ) {
        
        Setting* c = (*i)->byName(name);
        if (c != NULL)
            return c;
    }
    
    return NULL;
}

void ConfigurationGroup::load(QSqlDatabase* db) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        (*i)->load(db);
}

void ConfigurationGroup::save(QSqlDatabase* db) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        (*i)->save(db);
}

QWidget* VerticalConfigurationGroup::configWidget(QWidget* parent,
                                                  const char* widgetName) {
    
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QVBoxLayout* layout = new QVBoxLayout(widget, 18);
    widget->setTitle(getLabel());
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            layout->add(children[i]->configWidget(widget, NULL));
        
    return widget;
}

QWidget* HorizontalConfigurationGroup::configWidget(QWidget* parent,
                                                    const char* widgetName) {

    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QHBoxLayout* layout = new QHBoxLayout(widget, 20);
    widget->setTitle(getLabel());
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            layout->add(children[i]->configWidget(widget, NULL));

    return widget;
}

QWidget* StackedConfigurationGroup::configWidget(QWidget* parent,
                                                 const char* widgetName) {
    QWidgetStack* widget = new QWidgetStack(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addWidget(children[i]->configWidget(widget, NULL), i);

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    return widget;
}

QWidget* TabbedConfigurationGroup::configWidget(QWidget* parent,
                                                const char* widgetName) {
    QTabDialog* widget = new QTabDialog(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addTab(children[i]->configWidget(widget), children[i]->getLabel());

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

void TriggeredConfigurationGroup::setTrigger(Configurable* _trigger) {
    trigger = _trigger;
    // Make sure the stack is after the trigger
    addChild(configStack = new StackedConfigurationGroup());

    connect(trigger, SIGNAL(valueChanged(const QString&)),
            this, SLOT(triggerChanged(const QString&)));
}

void TriggeredConfigurationGroup::addTarget(QString triggerValue, Configurable* target) {
    configStack->addChild(target);
    triggerMap[triggerValue] = target;
}

void StringSelectSetting::addSelection(const QString& label, QString value, bool select) {
    if (value == QString::null)
        value = label;
    
    labels.push_back(label);
    values.push_back(value);
    
    emit selectionAdded(label, value);
    
    if (select || !isSet)
        setValue(value);
}

void StringSelectSetting::clearSelections(void) {
    labels.clear();
    values.clear();
    isSet = false;
    emit selectionsCleared();
}

void StringSelectSetting::setValue(const QString& newValue)  {
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

QWidget* LabelSetting::configWidget(QWidget* parent,
                                    const char* widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(widget);
    label->setText(getLabel() + ":");
    label->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* value = new QLabel(widget);
    value->setText(getValue());
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* LineEditSetting::configWidget(QWidget* parent,
                                       const char *widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(widget);
    label->setText(getLabel() + ":");
    label->setBackgroundOrigin(QWidget::WindowOrigin);

    QLineEdit* edit = new MythLineEdit(settingValue, widget,
                                    QString(widgetName) + "-edit");
    edit->setBackgroundOrigin(QWidget::WindowOrigin);
    edit->setText( getValue() );

    connect(this, SIGNAL(valueChanged(const QString&)),
            edit, SLOT(setText(const QString&)));
    connect(edit, SIGNAL(textChanged(const QString&)),
            this, SLOT(setValue(const QString&)));

    return widget;
}

QWidget* SliderSetting::configWidget(QWidget* parent,
                                     const char* widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");
    label->setBackgroundOrigin(QWidget::WindowOrigin);

    QSlider* slider = new MythSlider(widget, QString(widgetName) + "-slider");
    slider->setMinValue(min);
    slider->setMaxValue(max);
    slider->setOrientation(QSlider::Horizontal);
    slider->setLineStep(step);
    slider->setValue(intValue());

    QLCDNumber* lcd = new QLCDNumber(widget, QString(widgetName) + "-lcd");
    lcd->setMode(QLCDNumber::DEC);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->display(intValue());

    connect(slider, SIGNAL(valueChanged(int)), lcd, SLOT(display(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

    return widget;
}

QWidget* SpinBoxSetting::configWidget(QWidget* parent,
                                      const char* widgetName) {

    QWidget* box = new QHBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(box);
    label->setBackgroundOrigin(QWidget::WindowOrigin);
    label->setText(getLabel() + ":");

    QSpinBox* spinbox = new MythSpinBox(box);
    spinbox->setBackgroundOrigin(QWidget::WindowOrigin);
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    spinbox->setLineStep(step);
    spinbox->setValue(intValue());

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), spinbox, SLOT(setValue(int)));

    return box;
}

QWidget* SelectLabelSetting::configWidget(QWidget* parent,
                                          const char* widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(widget);
    label->setText(getLabel() + ":");
    label->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* value = new QLabel(widget);
    value->setText(labels[current]);
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* ComboBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    QWidget* box = new QHBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(box);
    label->setText(getLabel() + ":");
    label->setBackgroundOrigin(QWidget::WindowOrigin);
    QComboBox* widget = new MythComboBox(rw, box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
    }
    if (isSet)
        widget->setCurrentItem(current);

    connect(widget, SIGNAL(highlighted(int)),
            this, SLOT(setValue(int)));
    connect(this, SIGNAL(selectionAdded(const QString&,QString)),
            widget, SLOT(insertItem(const QString&)));
    connect(this, SIGNAL(connectionsCleared(void)),
            widget, SLOT(clear()));

    return box;
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

QWidget* RadioSetting::configWidget(QWidget* parent,
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

    return widget;
}

QWidget* CheckBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    
    QCheckBox* widget = new MythCheckBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setText(getLabel());
    widget->setChecked(boolValue());

    connect(widget, SIGNAL(toggled(bool)),
            this, SLOT(setValue(bool)));
    connect(this, SIGNAL(valueChanged(bool)),
            widget, SLOT(setChecked(bool)));

    return widget;
}

void ConfigurationDialogWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Key_Enter:
    case Key_Return:
    case Key_Space:
        accept();
        break;
    case Key_Escape:
        reject();
        break;
    default:
        e->ignore();
    }
}

QDialog* ConfigurationDialog::dialogWidget(QWidget* parent,
                                           const char* widgetName) {
    QDialog* dialog = new ConfigurationDialogWidget(parent, widgetName);
    QVBoxLayout* layout = new QVBoxLayout(dialog, 20);
    layout->addWidget(configWidget(dialog));

    return dialog;
}

int ConfigurationDialog::exec(QSqlDatabase* db) {
    load(db);

    QDialog* dialog = dialogWidget(NULL);

    float wmult = 0, hmult = 0;
    int screenheight = 0, screenwidth = 0;
    m_context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    dialog->setGeometry(0, 0, screenwidth, screenheight);
    dialog->setFixedSize(QSize(screenwidth, screenheight));

    dialog->setFont(QFont("Arial", (int)(m_context->GetSmallFontSize()*hmult),
                    QFont::Bold));

    m_context->ThemeWidget(dialog);

    dialog->showFullScreen();

    int ret;

    if ((ret = dialog->exec()) == QDialog::Accepted)
        save(db);

    delete dialog;

    return ret;
}

QDialog* ConfigurationWizard::dialogWidget(QWidget* parent,
                                           const char* widgetName) {
    QWizard* wizard = new QWizard(parent, widgetName, TRUE, 0);

    unsigned i;
    for(i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible()) {
            QWidget* child = children[i]->configWidget(parent);
            wizard->addPage(child, children[i]->getLabel());
            wizard->setHelpEnabled(child, false);
            if (i == children.size()-1)
                // Last page always has finish enabled for now
                // stuff should have sane defaults anyway
                wizard->setFinishEnabled(child, true);
        }
        
    return wizard;
}

QWidget* ConfigurationWizard::configWidget(QWidget* parent,
                                           const char* widgetName) {
    return dialogWidget(parent, widgetName);
}

void SimpleDBStorage::load(QSqlDatabase* db) {
    QString querystr = QString("SELECT %1 FROM %2 WHERE %3")
        .arg(column).arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        query.next();
        QString result = query.value(0).toString();
        if (result != QString::null)
          setValue(result);
    }
}

void SimpleDBStorage::save(QSqlDatabase* db) {
    QString querystr = QString("SELECT * FROM %1 WHERE %2")
        .arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        // Row already exists
        querystr = QString("UPDATE %1 SET %2 WHERE %4")
            .arg(table).arg(setClause()).arg(whereClause());
        query = db->exec(querystr);
    } else {
        // Row does not exist yet
        querystr = QString("INSERT INTO %1 SET %2")
            .arg(table).arg(setClause());
        query = db->exec(querystr);
    }
}

void AutoIncrementStorage::save(QSqlDatabase* db) {
    if (intValue() == 0) {
        // Generate a new, unique ID
        QString query = QString("INSERT INTO %1 (%2) VALUES (0)").arg(table).arg(column);
        QSqlQuery result = db->exec(query);
        if (!result.isActive() || result.numRowsAffected() < 1) {
            cout << "Failed to insert new entry for ("
                 << table << "," << column << ")\n";
            return;
        }
        result = db->exec("SELECT LAST_INSERT_ID()");
        if (!result.isActive() || result.numRowsAffected() < 1) {
            cout << "Failed to fetch last insert id" << endl;
            return;
        }

        result.next();
        setValue(result.value(0).toInt());
    }
}

QWidget* ListBoxSetting::configWidget(QWidget* parent, const char* widgetName) {
    QWidget* box = new QVBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel* label = new QLabel(box);
    label->setText(getLabel());
    label->setBackgroundOrigin(QWidget::WindowOrigin);
    MythListBox* widget = new MythListBox(box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
        if (isSet && current == i)
            widget->setCurrentItem(i);
    }

    // xxx, needs to update the listbox when a selection is added
//     connect(this, SIGNAL(selectionAdded(const QString&, QString)),
//             widget, SLOT(insertItem(QString)));
    connect(this, SIGNAL(valueChanged(const QString&)),
            widget, SLOT(setCurrentItem(const QString&)));
    connect(widget, SIGNAL(highlighted(const QString&)),
            this, SLOT(setValueByLabel(const QString&)));

    return box;
}

void ListBoxSetting::setValueByLabel(const QString& label) {
    for(unsigned i = 0 ; i < labels.size() ; ++i)
        if (labels[i] == label) {
            setValue(values[i]);
            return;
        }
    cerr << "BUG: ListBoxSetting::setValueByLabel called for unknown label " << label << endl;
}
