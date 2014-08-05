// -*- Mode: c++ -*-

#include <vector>
using std::vector;

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
