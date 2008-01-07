#include <algorithm>

#include <qwidgetstack.h>
#include <qtabdialog.h>

#include "mythconfiggroups.h"

static void clear_widgets(vector<Configurable*> &children,
                          vector<QWidget*>      &childwidget)
{
    for (uint i = 0; (i < childwidget.size()) && (i < children.size()); i++)
    {
        if (children[i] && childwidget[i])
            children[i]->widgetInvalid(childwidget[i]);
    }
    childwidget.clear();
}

ConfigurationGroup::~ConfigurationGroup()
{
    childList::iterator it = children.begin();
    for (; it != children.end() ; ++it)
    {
        if (*it)
        {
            (*it)->disconnect();
            (*it)->deleteLater();
        }
    }
    children.clear();
}

void ConfigurationGroup::deleteLater(void)
{
    childList::iterator it = children.begin();
    for (; it != children.end() ; ++it)
    {
        if (*it)
            (*it)->disconnect();
    }
    Setting::deleteLater();
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
    widget = new QGroupBox(parent, widgetName);
    connect(widget, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    float wmult = 0, hmult = 0;
    gContext->GetScreenSettings(wmult, hmult);

    int space  = (zeroSpace) ? 4 : -1;
    int margin = (int) ((uselabel) ? (28 * hmult) : (10 * hmult));
    margin = (zeroMargin) ? 4 : margin;

    layout = new QVBoxLayout(widget, margin, space);

    if (uselabel)
        widget->setTitle(getLabel());

    childwidget.resize(children.size());
    for (uint i = 0; i < children.size(); i++)
    {
        if (children[i] && children[i]->isVisible())
        {
            childwidget[i] = children[i]->configWidget(cg, widget, NULL);
            layout->add(childwidget[i]);
            children[i]->setEnabled(children[i]->isEnabled());
        }
    }
      
    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)),
                cg,   SIGNAL(changeHelpText(QString)));
        confgrp = cg;
    } 

    return widget;
}

void VerticalConfigurationGroup::widgetInvalid(QObject *obj)
{
    widget = (widget == obj) ? NULL : widget;
}

void VerticalConfigurationGroup::deleteLater(void)
{
    clear_widgets(children, childwidget);
    ConfigurationGroup::deleteLater();
}

bool VerticalConfigurationGroup::replaceChild(
    Configurable *old_child, Configurable *new_child)
{
    childList::iterator it = children.begin();
    for (uint i = 0; it != children.end(); ++it, ++i)
    {
        if (*it != old_child)
            continue;

        *it = new_child;

        if (!widget)
        {
            old_child->deleteLater();
            return true;
        }

        if (childwidget[i])
        {
            old_child->widgetInvalid(childwidget[i]);
            layout->remove(childwidget[i]);
            childwidget[i]->deleteLater();
            childwidget[i] = NULL;
        }

        bool was_visible = old_child->isVisible();
        bool was_enabled = old_child->isEnabled();

        old_child->deleteLater();

        if (was_visible)
        {
            childwidget[i] = new_child->configWidget(confgrp, widget, NULL);
            layout->add(childwidget[i]);
            new_child->setEnabled(was_enabled);
            childwidget[i]->resize(1,1);
            childwidget[i]->show();
        }

        return true;
    }

    return false;
}

void VerticalConfigurationGroup::repaint(void)
{
    if (widget)
        widget->repaint();
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

StackedConfigurationGroup::~StackedConfigurationGroup()
{
    clear_widgets(children, childwidget);
    ConfigurationGroup::deleteLater();
}

void StackedConfigurationGroup::deleteLater(void)
{
    clear_widgets(children, childwidget);
    ConfigurationGroup::deleteLater();
}

QWidget* StackedConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                 QWidget* parent,
                                                 const char* widgetName) 
{
    widget = new QWidgetStack(parent, widgetName);
    connect(widget, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for (uint i = 0 ; i < children.size() ; ++i)
    {
        if (children[i]->isVisible())
        {
            childwidget[i] = children[i]->configWidget(cg, widget, NULL);
            widget->addWidget(childwidget[i], i);
        }
    }

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }
    confgrp = cg;

    return widget;
}

void StackedConfigurationGroup::widgetInvalid(QObject *obj)
{
    widget = (widget == obj) ? NULL : widget;
}

void StackedConfigurationGroup::addChild(Configurable *child)
{
    ConfigurationGroup::addChild(child);
    childwidget.resize(childwidget.size() + 1);
    if (!widget)
        return;

    uint i = children.size() - 1;
    if ((i < children.size()) && children[i]->isVisible())
    {
        childwidget[i] = children[i]->configWidget(confgrp, widget, NULL);
        widget->addWidget(childwidget[i], i);
        childwidget[i]->resize(1,1);
        childwidget[i]->show();
    }
}

void StackedConfigurationGroup::removeChild(Configurable *child)
{
    childList::iterator it = find(children.begin(), children.end(), child);
    if (it == children.end())
        return;

    uint i = it - children.begin();
    if ((i >= children.size()) || (i >= childwidget.size()))
        return;

    children.erase(it);
    
    vector<QWidget*>::iterator cit = childwidget.begin() + i;
    QWidget *cw = *cit;
    childwidget.erase(cit);

    if (widget && cw)
    {
        child->widgetInvalid(cw);
        widget->removeWidget(cw);
    }
}

void StackedConfigurationGroup::raise(Configurable* child)
{
    for (uint i = 0 ; i < children.size() ; i++)
    {
        if (children[i] == child)
        {
            top = i;
            emit raiseWidget((int)i);
            return;
        }
    }

    VERBOSE(VB_IMPORTANT, "BUG: StackedConfigurationGroup::raise(): "
            "unrecognized child " << child << " "
            <<QString("on setting %1/%2").arg(getName()).arg(getLabel()));
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
    triggerMap[QDeepCopy<QString>(triggerValue)] = target;

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

    if (!widget && configStack)
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

void TriggeredConfigurationGroup::repaint(void)
{
    VerifyLayout();

    if (widget)
        widget->repaint();
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

void TriggeredConfigurationGroup::removeTarget(QString triggerValue)
{
    HostComboBox *combobox = dynamic_cast<HostComboBox*>(trigger);
    if (!combobox)
        return;

    QMap<QString,Configurable*>::iterator cit = triggerMap.find(triggerValue);
    if (cit == triggerMap.end())
        return;

    // remove trigger value from trigger combobox
    combobox->removeSelection(triggerValue);

    // actually remove the pane
    configStack->removeChild(*cit);
    triggerMap.erase(cit);
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
    connect(widget, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    return widget;
}

void TriggeredConfigurationGroup::widgetInvalid(QObject *obj)
{
    widget = (widget == obj) ? NULL : widget;
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
