
#include "libmythbase/mythlogging.h"

#include "mythmainwindow.h"
#include "mythuispinbox.h"
#include "mythuibutton.h"
#include "mythuitextedit.h"
#include "mythuitext.h"

// QT headers
#include <QCoreApplication>
#include <QDomDocument>
#include <utility>

/**
 * \brief Set the lower and upper bounds of the spinbox, the interval and page
 *        amount
 *
 * \param low  The lowest value
 * \param high The highest value
 * \param step The interval between displayed values e.g. sequence 0,5,10,15
 *             would have have step value of 5
 * \param pageMultiple The number of list items to move by when PAGEUP and
 *             PAGEDOWN input actions are used
 */
void MythUISpinBox::SetRange(int low, int high, int step, uint pageMultiple)
{
    if ((high == low) || step == 0)
        return;

    m_low = low;
    m_high = high;
    m_step = step;

    m_moveAmount = pageMultiple;

    bool reverse = false;
    int value = low;

    if (low > high)
        reverse = true;

    Reset();

    while ((reverse && (value >= high)) ||
           (!reverse && (value <= high)))
    {
        QString text;

        if (m_hasTemplate)
        {
            QString temp;

            if (value < 0 && !m_negativeTemplate.isEmpty())
                temp = m_negativeTemplate;
            else if (value == 0 && !m_zeroTemplate.isEmpty())
                temp = m_zeroTemplate;
            else if (!m_positiveTemplate.isEmpty())
                temp = m_positiveTemplate;

            if (!temp.isEmpty())
            {
                if (temp.contains("%n"))
                {
                    text = QCoreApplication::translate("ThemeUI", temp.toUtf8(), nullptr,
                                           qAbs(value));
                }
                else
                {
                    text = QCoreApplication::translate("ThemeUI", temp.toUtf8());
                }
            }
        }

        if (text.isEmpty())
            text = QString::number(value);

        new MythUIButtonListItem(this, text, QVariant::fromValue(value));

        if (reverse)
            value = value - step;
        else
            value = value + step;
    }

    CalculateArrowStates();
}


/**
 * \brief Add a special label for a value of the spinbox, it does not need to be in the range
 *
 * \param label The text displayed
 * \param value The value associated to this label
 */
void MythUISpinBox::AddSelection(int value, const QString &label)
{
    if (!label.isEmpty())
    {
        MythUIButtonListItem *item = GetItemByData(value);
        if (item)
        {
            item->SetText(label);
            return;
        }
    }

    int insertPos=-1;

    for (int pos = 0; pos < m_itemList.size(); pos++)
    {
        MythUIButtonListItem *item = m_itemList.at(pos);
        if (item->GetData().toInt() > value)
        {
            insertPos = pos;
            break;
        }
    }

    new MythUIButtonListItem(this, label.isEmpty() ? QChar(value) : label,
                                    QVariant::fromValue(value), insertPos);
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUISpinBox::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "template")
    {
        QString format = parseText(element);

        if (element.attribute("type") == "negative")
            m_negativeTemplate = format;
        else if (element.attribute("type") == "zero")
            m_zeroTemplate = format;
        else
            m_positiveTemplate = format;

        m_hasTemplate = true;
    }
    else
    {
        return MythUIButtonList::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIButtonList::MoveDown()
 */
bool MythUISpinBox::MoveDown(MovementUnit unit, uint amount)
{
    bool handled = false;

    if ((unit == MovePage) && m_moveAmount)
        handled = MythUIButtonList::MoveDown(MoveByAmount, m_moveAmount);
    else
        handled = MythUIButtonList::MoveDown(unit, amount);

    return handled;
}

/**
 *  \copydoc MythUIButtonList::MoveUp()
 */
bool MythUISpinBox::MoveUp(MovementUnit unit, uint amount)
{
    bool handled = false;

    if ((unit == MovePage) && m_moveAmount)
        handled = MythUIButtonList::MoveUp(MoveByAmount, m_moveAmount);
    else
        handled = MythUIButtonList::MoveUp(unit, amount);

    return handled;
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUISpinBox::CreateCopy(MythUIType *parent)
{
    auto *spinbox = new MythUISpinBox(parent, objectName());
    spinbox->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUISpinBox::CopyFrom(MythUIType *base)
{
    auto *spinbox = dynamic_cast<MythUISpinBox *>(base);

    if (!spinbox)
        return;

    m_hasTemplate = spinbox->m_hasTemplate;
    m_negativeTemplate = spinbox->m_negativeTemplate;
    m_zeroTemplate = spinbox->m_zeroTemplate;
    m_positiveTemplate = spinbox->m_positiveTemplate;

    MythUIButtonList::CopyFrom(base);
}

// Open the entry dialog on certain key presses. A select or search action will
// open the dialog. A number or minus sign will open the entry dialog with the
// given key as the first digit.
// If the spinbox uses a template, the entries are not just numbers
// but can be sentences. The whole sentence is put in the entry field,
// allowing the user to change the number part of it.

bool MythUISpinBox::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);
    if (handled)
        return true;

    MythUIButtonListItem *item = GetItemCurrent();
    if (item == nullptr)
        return MythUIButtonList::keyPressEvent(event);

    QString initialEntry = item->GetText();
    bool doEntry = false;

    // Only invoke the entry dialog if the entry is a number
    bool isNumber = false;
    (void)initialEntry.toLongLong(&isNumber,10);
    if (!isNumber)
        return MythUIButtonList::keyPressEvent(event);

    for (const QString& action : std::as_const(actions))
    {
        if (action >= ACTION_0 && action <= ACTION_9)
        {
            if (!m_hasTemplate)
                initialEntry = action;
            doEntry=true;
            break;
        }
        if (action == ACTION_SELECT || action == "SEARCH")
        {
            doEntry=true;
            break;
        }
    }
    if (actions.empty() && event->text() == "-")
    {
        if (!m_hasTemplate)
            initialEntry = "-";
        doEntry=true;
    }

    if (doEntry)
    {
        ShowEntryDialog(initialEntry);
        handled = true;
    }

    if (handled)
        return true;
    return MythUIButtonList::keyPressEvent(event);
}

void MythUISpinBox::ShowEntryDialog(QString initialEntry)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *dlg = new SpinBoxEntryDialog(popupStack, "SpinBoxEntryDialog",
        this, std::move(initialEntry), m_low, m_high, m_step);

    if (dlg->Create())
        popupStack->AddScreen(dlg);
    else
        delete dlg;
}

// Convenience Dialog to allow entry of a Spinbox value

SpinBoxEntryDialog::SpinBoxEntryDialog(MythScreenStack *parent, const char *name,
        MythUIButtonList *parentList, QString searchText,
        int low, int high, int step)
    : MythScreenType(parent, name, false),
        m_parentList(parentList),
        m_searchText(std::move(searchText)),
        m_selection(parentList->GetCurrentPos()),
        m_low(low),
        m_high(high),
        m_step(step)
{
}


bool SpinBoxEntryDialog::Create(void)
{
    if (!CopyWindowFromBase("SpinBoxEntryDialog", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_entryEdit,  "entry", &err);
    UIUtilW::Assign(this, m_cancelButton,"cancel", &err);
    UIUtilW::Assign(this, m_rulesText,"rules", &err);
    UIUtilE::Assign(this, m_okButton,    "ok", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'SpinBoxEntryDialog'");
        return false;
    }

    m_entryEdit->SetText(m_searchText);
    entryChanged();
    if (m_rulesText)
    {
        InfoMap infoMap;
        infoMap["low"] = QString::number(m_low);
        infoMap["high"] = QString::number(m_high);
        infoMap["step"] = QString::number(m_step);
        m_rulesText->SetTextFromMap(infoMap);
    }

    connect(m_entryEdit, &MythUITextEdit::valueChanged, this, &SpinBoxEntryDialog::entryChanged);
    if (m_cancelButton)
        connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_okButton, &MythUIButton::Clicked, this, &SpinBoxEntryDialog::okClicked);

    BuildFocusList();

    return true;
}

void SpinBoxEntryDialog::entryChanged(void)
{
    int currPos = 0;
    int count = m_parentList->GetCount();
    QString searchText = m_entryEdit->GetText();
    bool found = false;
    for (currPos = 0; currPos < count; currPos++)
    {
        if (searchText.compare(m_parentList->GetItemAt(currPos)->GetText(),
            Qt::CaseInsensitive) == 0)
        {
            found = true;
            m_selection = currPos;
            break;
        }
    }
    m_okButton->SetEnabled(found);
}

void SpinBoxEntryDialog::okClicked(void)
{
    m_parentList->SetItemCurrent(m_selection);
    Close();
}
