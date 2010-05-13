// -*- Mode: c++ -*-

#include "mythconfigdialogs.h"
#include "mythwizard.h"

#include "mythuihelper.h"

#include <QWidget>
#include <QHBoxLayout>

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

void ConfigPopupDialogWidget::keyPressEvent(QKeyEvent* e)
{
    switch (e->key())
    {
        case Qt::Key_Escape:
            reject();
            emit popupDone(MythDialog::Rejected);
            break;
        default:
            MythDialog::keyPressEvent(e);
    }
}

void ConfigurationPopupDialog::deleteLater(void)
{
    disconnect();
    if (dialog)
    {
        dialog->disconnect();
        dialog->deleteLater();
        dialog = NULL;
        label = NULL;
    }
    VerticalConfigurationGroup::deleteLater();
}

MythDialog* ConfigurationPopupDialog::dialogWidget(MythMainWindow* parent,
                                                   const char* widgetName)
{
    dialog = new ConfigPopupDialogWidget(parent, widgetName);

    if (getLabel() != "")
    {
        label = new QLabel();
        label->setText(getLabel());
        label->setAlignment(Qt::AlignHCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Minimum,
                                         QSizePolicy::Maximum));

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(label);

        QWidget *box = new QWidget(dialog);
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum,
                                       QSizePolicy::Maximum));
        box->setLayout(layout);

        dialog->addWidget(box);
    }

    QWidget *widget = configWidget(NULL, dialog, "ConfigurationPopup");
    dialog->addWidget(widget);
    widget->setFocus();

    return dialog;
}

void ConfigurationPopupDialog::setLabel(QString str)
{
    VerticalConfigurationGroup::setLabel(str);
    if (label)
        label->setText(str);
}

DialogCode ConfigurationPopupDialog::exec(bool saveOnAccept)
{
    storage->Load();

    dialog = (ConfigPopupDialogWidget*)
        dialogWidget(GetMythMainWindow(), "ConfigurationPopupDialog");
    dialog->ShowPopup(this);

    DialogCode ret = dialog->exec();

    if ((QDialog::Accepted == ret) && saveOnAccept)
        storage->Save();

    return ret;
}

void ConfigurationDialogWidget::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString &action = actions[i];
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

    if (!handled)
        MythDialog::keyPressEvent(e);
}

ConfigurationDialog::~ConfigurationDialog()
{
    clear_widgets(cfgChildren, childwidget);
    cfgGrp->deleteLater();
}

MythDialog* ConfigurationDialog::dialogWidget(MythMainWindow *parent,
                                              const char *widgetName)
{
    dialog = new ConfigurationDialogWidget(parent, widgetName);

    float wmult = 0, hmult = 0;

    GetMythUI()->GetScreenSettings(wmult, hmult);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setSpacing((int)(20 * hmult));

    ChildList::iterator it = cfgChildren.begin();
    childwidget.clear();
    childwidget.resize(cfgChildren.size());
    for (uint i = 0; it != cfgChildren.end(); ++it, ++i)
    {
        if ((*it)->isVisible())
        {
            childwidget[i] = (*it)->configWidget(cfgGrp, dialog);
            layout->addWidget(childwidget[i]);
        }
    }

    return dialog;
}

DialogCode ConfigurationDialog::exec(bool saveOnAccept, bool doLoad)
{
    if (doLoad)
        Load();

    MythDialog *dialog = dialogWidget(
        GetMythMainWindow(), "Configuration Dialog");

    dialog->Show();

    DialogCode ret = dialog->exec();

    if ((QDialog::Accepted == ret) && saveOnAccept)
        Save();

    clear_widgets(cfgChildren, childwidget);

    dialog->deleteLater();
    dialog = NULL;

    return ret;
}

void ConfigurationDialog::addChild(Configurable *child)
{
    cfgChildren.push_back(child);
    cfgGrp->addChild(child);
}

void ConfigurationDialog::setLabel(const QString &label)
{
    if (label.isEmpty())
    {
        cfgGrp->setUseLabel(false);
        cfgGrp->setLabel("");
    }
    else
    {
        cfgGrp->setLabel(label);
        cfgGrp->setUseLabel(true);
        cfgGrp->setUseFrame(true);
    }
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

JumpConfigurationWizard::~JumpConfigurationWizard()
{
    clear_widgets(cfgChildren, childwidget);
}

void JumpConfigurationWizard::deleteLater(void)
{
    clear_widgets(cfgChildren, childwidget);
    QObject::deleteLater();
}

MythDialog *JumpConfigurationWizard::dialogWidget(MythMainWindow *parent,
                                                  const char *widgetName)
{
    MythJumpWizard *wizard = new MythJumpWizard(parent, widgetName);
    dialog = wizard;

    QObject::connect(cfgGrp, SIGNAL(changeHelpText(QString)),
                     wizard, SLOT(  setHelpText(   QString)));

    childwidget.clear();
    QStringList labels, helptext;
    for (uint i = 0; i < cfgChildren.size(); i++)
    {
        if (cfgChildren[i]->isVisible())
        {
            childwidget.push_back(
                cfgChildren[i]->configWidget(cfgGrp, parent));
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

    for (uint i = 0; i < childwidget.size(); i++)
    {
        wizard->addPage(childwidget[i], labels[i]);
        wizard->setFinishEnabled(childwidget[i], true);
    }

    return wizard;
}

void JumpConfigurationWizard::showPage(QString page)
{
    uint pagenum = page.toUInt();
    if (pagenum >= childwidget.size() || !dialog)
        return;
    ((MythJumpWizard*)(dialog))->showPage(childwidget[pagenum]);
}

TerminalWizard::TerminalWizard(QString program, QStringList args) :
    terminal(new MythTerminal(program, args))
{
    addChild(terminal);
}

DialogCode TerminalWizard::exec(bool saveOnExec, bool doLoad)
{
    terminal->Start();
    return ConfigurationWizard::exec(saveOnExec, doLoad);
}
