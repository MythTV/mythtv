#include "settings.h"
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

QWidget* VerticalConfigurationGroup::configWidget(QWidget* parent,
                                                  const char* widgetName) {
    
    //QVGroupBox* widget = new QVGroupBox(parent, widgetName);
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    QVBoxLayout* layout = new QVBoxLayout(widget, 20);
    widget->setTitle(getLabel());
    for(unsigned i = 0 ; i < children.size() ; ++i)
        layout->add(children[i]->configWidget(widget, NULL));
        
    return widget;
}

QWidget* StackedConfigurationGroup::configWidget(QWidget* parent,
                                                 const char* widgetName) {
    QWidgetStack* widget = new QWidgetStack(parent, widgetName);

    unsigned i;
    for(i = 0 ; i < children.size() ; ++i)
        widget->addWidget(children[i]->configWidget(widget, NULL), i);

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    return widget;
}

void StackedConfigurationGroup::raise(Configurable* child) {
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i] == child) {
            top = i;
            emit raiseWidget((int)i);
            return;
        }
    cerr << "BUG: StackedConfigurationGroup::raise(): unrecognized child " << child << endl;
}

QWidget* LineEditSetting::configWidget(QWidget* parent,
                                       const char *widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");

    QLineEdit* edit = new QLineEdit(settingValue, widget,
                                    QString(widgetName) + "-edit");
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

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");

    QSlider* slider = new QSlider(widget, QString(widgetName) + "-slider");
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

    QWidget* widget = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");

    QSpinBox* spinbox = new QSpinBox(widget, QString(widgetName) + "-spinbox");
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    spinbox->setLineStep(step);
    spinbox->setValue(intValue());

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), spinbox, SLOT(setValue(int)));

    return widget;
}

QWidget* ComboBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    QComboBox* widget = new QComboBox(rw, parent, widgetName);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
    }
    if (isSet)
        widget->setCurrentItem(current);

    connect(widget, SIGNAL(highlighted(int)),
            this, SLOT(setValue(int)));

    return widget;
}

QWidget* RadioSetting::configWidget(QWidget* parent,
                                    const char* widgetName) {
    QButtonGroup* widget = new QButtonGroup(parent, widgetName);
    widget->setTitle(getLabel());

    for( unsigned i = 0 ; i < labels.size() ; ++i ) {
        QRadioButton* button = new QRadioButton(widget, NULL);
        button->setText(labels[i]);
        if (isSet && i == current)
            button->setDown(true);
    }

    return widget;
}

QWidget* CheckBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    
    QCheckBox* widget = new QCheckBox(parent, widgetName);
    widget->setText(getLabel());

    return widget;
}


QWidget* ConfigurationWizard::configWidget(QWidget* parent,
                                           const char* widgetName) {
    QWizard* wizard = new QWizard(parent, widgetName, FALSE, 0);

    wizard->resize(600, 480); // xxx

    unsigned i;
    for(i = 0 ; i < children.size() ; ++i) {
        QWidget* child = children[i]->configWidget(parent);
        wizard->addPage(child, pageTitles[i]);
        if (i == children.size()-1)
            // Last page always has finish enabled for now
            // stuff should have sane defaults anyway
            wizard->setFinishEnabled(child, true);
    }

    wizard->showPage(wizard->page(0));
    return wizard;
}


void SimpleDBStorage::load(QSqlDatabase* db) {
    QString querystr = QString("SELECT %1 FROM %2 WHERE %3")
        .arg(column).arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        query.next();
        QVariant result = query.value(0);
        setValue(query.value(0).toString());
    }
}

void SimpleDBStorage::purge(QSqlDatabase* db) {
    QString querystr = QString("DELETE FROM %1 WHERE %2")
        .arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);
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
