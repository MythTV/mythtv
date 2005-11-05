using namespace std;

#include "managedlist.h"
#include "mythcontext.h"
#include "xmlparse.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ManagedListItem
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ManagedListItem::ManagedListItem(const QString& startingText, ManagedList* _parentList, QObject* _parent, const char* _name)
               : QObject(_parent, _name)
{
    text = startingText;
    listIndex = 0;
    curState = MLS_NORMAL;
    enabled = true;
    parentList = _parentList;
    valueText = " ";
}

void ManagedListItem::setParentList(ManagedList* _parentList)
{
    parentList = _parentList;
    connect(this, SIGNAL(changed(ManagedListItem*)), parentList, SLOT(itemChanged(ManagedListItem*)));
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BoolManagedListItem
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


BoolManagedListItem::BoolManagedListItem(bool _initialValue, ManagedListGroup* pGroup,
                                         ManagedList* pList, QObject* _parent, const char* _name)
                   : SelectManagedListItem("", pGroup, pList, _parent, _name)
{
    initialValue = _initialValue;
    listBuilt = false;
}

void BoolManagedListItem::setLabels(const QString& _trueLabel, const QString& _falseLabel)
{
    trueLabel = _trueLabel;
    falseLabel = _falseLabel;
    setValue(initialValue);
}

void BoolManagedListItem::generateList()
{
    if (!listBuilt)
    {
        addSelection(trueLabel, "1", false);
        addSelection(falseLabel, "0", false);
        listBuilt = true;
        selectValue(valueText);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntegerListItem
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IntegerManagedListItem::IntegerManagedListItem(int _bigStep, int _step, ManagedList* pList, QObject* _parent, const char* _name)
                      : ManagedListItem("", pList, _parent, _name)
{
    step = _step;
    bigStep = _bigStep;

    setTemplates("-%1","-%1", "%1", "%1", "%1");
    setShortTemplates("-%1","-%1", "%1", "%1", "%1");

    ManagedListItem::setValue(QString("0"));
}


void IntegerManagedListItem::setTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                                          const QString& oneStr, const QString& posStr)
{
    negTemplate = negStr;
    negOneTemplate = negOneStr;
    posTemplate = posStr;
    posOneTemplate = oneStr;
    zeroTemplate = zeroStr;
    syncTextToValue();
}

void IntegerManagedListItem::setShortTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                                               const QString& oneStr, const QString& posStr)
{
    shortNegTemplate = negStr;
    shortNegOneTemplate = negOneStr;
    shortPosTemplate = posStr;
    shortPosOneTemplate = oneStr;
    shortZeroTemplate = zeroStr;
    syncTextToValue();
}

#define ASSIGN_TEMPLATE(x,y,z) \
    if (x.find("%1") != -1) \
        y = QString(x).arg(abs(z)); \
    else \
        y = x;



void IntegerManagedListItem::syncTextToValue()
{
    int v = intValue();
    if (v == 0)
    {
        ASSIGN_TEMPLATE(zeroTemplate, text, v)
        ASSIGN_TEMPLATE(shortZeroTemplate, shortText, v)
    }
    else if (v == 1)
    {
        ASSIGN_TEMPLATE(posOneTemplate, text, v);
        ASSIGN_TEMPLATE(shortPosOneTemplate, shortText, v);
    }
    else if (v == -1)
    {
        ASSIGN_TEMPLATE(negOneTemplate, text, v);
        ASSIGN_TEMPLATE(shortNegOneTemplate, shortText, v);
    }
    else if (v > 0)
    {
        ASSIGN_TEMPLATE(posTemplate, text, v);
        ASSIGN_TEMPLATE(shortPosTemplate, shortText, v);
    }
    else
    {
        ASSIGN_TEMPLATE(negTemplate, text, v);
        ASSIGN_TEMPLATE(shortNegTemplate, shortText, v);
    }

    ManagedListItem::syncTextToValue();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BoundedIntegerListItem
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BoundedIntegerManagedListItem::BoundedIntegerManagedListItem(int _minVal, int _maxVal, int _bigStep,
                                                             int _step, ManagedListGroup* _group,
                                                             ManagedList* _list,
                                                             QObject* _parent, const char* _name,
                                                             bool _invert)
                             : SelectManagedListItem("", _group,  _list, _parent, _name)
{
    step = _step;
    bigStep = _bigStep;
    minVal = _minVal;
    maxVal = _maxVal;
    listBuilt = false;
    inverted = _invert;
//    setValue(0);
}


void BoundedIntegerManagedListItem::setTemplates(const QString& negStr, const QString& negOneStr,
                                                 const QString& zeroStr,
                                                 const QString& oneStr, const QString& posStr)
{
    negTemplate = negStr;
    negOneTemplate = negOneStr;
    posTemplate = posStr;
    posOneTemplate = oneStr;
    zeroTemplate = zeroStr;
//    generateList();
}

void BoundedIntegerManagedListItem::setValue(int val)
{
    if (val > maxVal)
        val = maxVal;
    else if ( val < minVal)
        val = minVal;

    SelectManagedListItem::setValue(QString::number(val));
}

void BoundedIntegerManagedListItem::generateList()
{
    if (!listBuilt)
    {
        for (int i = minVal; i <= maxVal; i++)
        {
            addSelection( numericToString(i), QString::number(i), false);
        }

        listBuilt = true;
        selectValue(valueText);
    }

}


QString BoundedIntegerManagedListItem::numericToString(int v)
{
    QString str;
    if (v == 0)
    {
        ASSIGN_TEMPLATE(zeroTemplate, str, v)
    }
    else if (v == 1)
    {
        ASSIGN_TEMPLATE(posOneTemplate, str, v);
    }
    else if (v == -1)
    {
        ASSIGN_TEMPLATE(negOneTemplate, str, v);
    }
    else if (v > 0)
    {
        ASSIGN_TEMPLATE(posTemplate, str, v);
    }
    else
    {
        ASSIGN_TEMPLATE(negTemplate, str, v);
    }

    return str;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ManagedListGroup
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ManagedListGroup::ManagedListGroup(const QString& txt, ManagedListGroup* pGroup, ManagedList* pList,
                                   QObject* _parent, const char* _name)
                : ManagedListItem(txt, pList, _parent, _name)
{
    parentGroup = pGroup;
    if (pGroup)
    {
        goBack = new ManagedListItem(QString("[ %1 ]").arg(QObject::tr("Go Back")), parentList, this, "goBack");
        goBack->setValue("__NO_VALUE__");
        goBack->setState(MLS_BOLD);
        goBack->setEnabled(true);
        addItem(goBack);
        connect(goBack, SIGNAL(selected(ManagedListItem*)), this, SLOT(doGoBack()));
        connect(goBack, SIGNAL(canceled(ManagedListItem*)), this, SLOT(doGoBack()));
    }
    else
        goBack = NULL;
    curItem = 0;
    itemCount = 0;

}

void ManagedListGroup::slotGuiActivate(ManagedListGroup* _group)
{
    if (_group != this)
        return;

    ManagedListItem* item = NULL;
    for (item = itemList.first(); item; item = itemList.next())
    {
        item->slotGuiActivate(_group);
    }
}

void ManagedListGroup::setCurIndex(int newVal)
{
    if (newVal < 0)
        newVal = 0;
    else if (newVal >= itemCount)
        newVal = itemCount - 1;

    curItem = newVal;
    valueText = QString::number(curItem);
    getCurItem()->gotFocus();

    changed();
}


bool ManagedListGroup::addItem(ManagedListItem* item, int where)
{
    if (!item)
        return false;

    if (item->name() == "unnamed")
        item->setName( QString( "ITEM-%1").arg(itemList.count()));

    if (!child(item->name()) && !item->parent())
        insertChild(item);

    int listSize = itemList.count();

    if ((where == -2) || (listSize ==0))
        itemList.append(item);
    else if (where == -1)
        itemList.insert(itemList.count() - 1, item);
    else
        itemList.insert(where, item);

    itemCount = itemList.count();
    if (parentList)
        connect(item, SIGNAL(changed(ManagedListItem*)), parentList, SLOT(itemChanged(ManagedListItem*)));
    return true;
}

void ManagedListGroup::doGoBack()
{
    emit goingBack();
    getParentList()->setCurGroup(parentGroup);
    emit wentBack();
}

void ManagedListGroup::cursorRight(bool)
{
    getParentList()->setCurGroup(this);
}

void ManagedListGroup::select()
{
    if (enabled)
        getParentList()->setCurGroup(this);
}

void ManagedListGroup::clear()
{
    ManagedListItem* tempItem = NULL;
    for(tempItem = itemList.first(); tempItem; tempItem = itemList.next() )
    {
        delete tempItem;
    }

    itemList.clear();

    if (parentGroup)
    {
        goBack = new ManagedListItem(QString("[ %1 ]").arg(QObject::tr("Go Back")), parentList, this, "goBack");
        goBack->setValue("__NO_VALUE__");
        addItem(goBack);
        connect(goBack, SIGNAL(selected(ManagedListItem*)), this, SLOT(doGoBack()));
        connect(goBack, SIGNAL(canceled(ManagedListItem*)), this, SLOT(doGoBack()));
    }

}


void ManagedListGroup::setParentList(ManagedList* _parent)
{
    ManagedListItem* tempItem = NULL;
    ManagedListItem::setParentList(_parent);
    for(tempItem = itemList.first(); tempItem; tempItem = itemList.next() )
    {
        tempItem->setParentList(_parent);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SelectManagedListItem
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SelectManagedListItem::SelectManagedListItem(const QString& baseTxt, ManagedListGroup* pGroup, ManagedList* pList,
                                             QObject* _parent, const char* _name)
                     : ManagedListGroup(baseTxt, pGroup, pList, _parent, _name)
{
    baseText = baseTxt;
    goBack->setText(QString("[ %1 ]").arg(QObject::tr("No Change")));
}

ManagedListItem* SelectManagedListItem::addSelection(const QString& label, QString value, bool selectit)
{
    ManagedListItem* ret = NULL;

    if (value == QString::null)
        value = label;

    bool found = false;

    for(ManagedListItem* tempItem = itemList.first(); tempItem; tempItem = itemList.next() )
    {
        if ((tempItem->getText() == label) || (tempItem->getValue() == value))
        {
            found = true;
            tempItem->setValue(value);
            tempItem->setText(label);
            ret = tempItem;
            break;
        }

    }

    if (!found)
    {
        ManagedListItem* newItem = new ManagedListItem(label, parentList, this, label);
        newItem->setValue(value);
        addItem(newItem);
        ret = newItem;
        connect(newItem, SIGNAL(selected(ManagedListItem*)), this, SLOT(itemSelected(ManagedListItem* )));
    }

    //cerr << "adding '" << label << "' value == " << value << " curval == " << valueText << endl;

    // If we're adding an item with the same value as what we have selected
    // go trhough the selection process so the list gets updated.
    if (value == valueText)
    {

        int index = getValueIndex(value);
        //cerr << "new item matches cur value and is at: " << index << endl;
        if (index > 0)
        {
            curItem = index;
            text = getCurItemText();
            setValue(value);
        }
    }
    else if (selectit)
        selectValue(value);


    emit selectionAdded(label, value);

    return ret;
}

ManagedListItem* SelectManagedListItem::addButton(const QString& label, QString value, bool selectit)
{
    ManagedListItem* newItem = new ManagedListItem(label, parentList, this, label);
    newItem->setValue(value);
    addItem(newItem);


    connect(newItem, SIGNAL(selected(ManagedListItem*)), this, SLOT(buttonSelected(ManagedListItem* )));
    if (selectit)
        selectValue(value);
    return newItem;
}


void SelectManagedListItem::clearSelections(void)
{
    ManagedListGroup::clear();
    isSet = false;

    text = baseText;
    emit selectionsCleared();
    changed();
}


void SelectManagedListItem::cursorRight(bool)
{
    if (!enabled)
        return;

    ++curItem;
    if (curItem >= (itemCount-1))
        curItem = 0;

    text = getCurItemText();
    valueText = getCurItemValue();

    changed();
}

void SelectManagedListItem::cursorLeft(bool)
{
    if (!enabled)
        return;

    --curItem;
    if (curItem < 0)
        curItem =  itemCount - 2;

    text = getCurItemText();
    valueText = getCurItemValue();
    changed();
}

void SelectManagedListItem::setValue(const QString& newValue)
{
    int index = getValueIndex(newValue);
    if ((curItem != index) && (index != -1))
    {
        curItem = getValueIndex(newValue);
    }
    text = getCurItemText();
    ManagedListGroup::setValue(newValue);
}


void SelectManagedListItem::select(const QString& newValue, bool bValue)
{
    int index = 1;

    if (bValue)
        index = getValueIndex(newValue);
    else
        index = getTextIndex(newValue);


    if (index > -1)
    {
        curItem = index;
        text = getCurItemText();
        setValue(getCurItemValue());
    }
}


void SelectManagedListItem::select()
{
    lastItem = curItem;
    ManagedListGroup::select();
}

void SelectManagedListItem::doGoBack()
{
    if (curItem == (itemCount - 1) )
        curItem = lastItem;
    else
        text = getCurItemText();

    valueText = getCurItemValue();
    text = getCurItemText();
    changed();


    ManagedListGroup::doGoBack();
}

void SelectManagedListItem::itemSelected(ManagedListItem*)
{
    doGoBack();
}

void SelectManagedListItem::buttonSelected(ManagedListItem* itm)
{
    parentList->setLocked();
    emit(buttonPressed(this, itm));
    doGoBack();
    parentList->setLocked(false);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ManagedList
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ManagedList::ManagedList(MythDialog* parent, const char* name) : QObject(parent, name)
{
    listRect = QRect(0, 0, 0, 0);
    theme = NULL;
    curGroup = NULL;
    locked = false;
}


void ManagedList::paintEvent(const QRect& r, QPainter *p, bool force)
{

    if (force || r.intersects(listRect))
        update(p);
}


void ManagedList::update(QPainter *p)
{

    LayerSet *container = theme->GetSet(containerName);
    if (container && curGroup)
    {

        int itemCount = curGroup->getItemCount();
        int curItem = curGroup->getCurIndex();


        QRect pr = listRect;
        QPixmap pix(pr.size());
        pix.fill(getParent(), pr.topLeft());
        QPainter tmp(&pix);

        QString tmptitle;



        UIListType *ltype = (UIListType *)container->GetType(listName);
        if (ltype)
        {
            ltype->ResetList();
            ltype->SetActive(true);
            int skip;
            if (itemCount <= listSize || curItem <= listSize / 2)
                skip = 0;
            else if (curItem >= itemCount - listSize + listSize / 2)
                skip = itemCount - listSize;
            else
                skip = curItem - listSize / 2;

            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listSize < itemCount);


            for (int i = 0; i < listSize; i++)
            {
                if (i + skip >= itemCount)
                    break;

                ManagedListItem *itm = curGroup->getItem(i+skip);

                ltype->SetItemText(i, 1, itm->getText());
                int arrowsWanted = 0;

                if (itm->hasLeft())
                {
                    arrowsWanted = UIListType::ARROW_LEFT;
                }

                if (itm->hasRight())
                {
                    arrowsWanted |= UIListType::ARROW_RIGHT;

                }

                ltype->SetItemArrow(i, arrowsWanted);
                ltype->SetItemText(i, itm->getText());

                int state = itm->getState();
                if (state == MLS_NORMAL)
                {
                    if ( !itm->getEnabled())
                        ltype->EnableForcedFont(i, "disabled");

                }
                else
                {
                    QString fntName;
                    if (itm->getEnabled())
                        fntName = QString("enabled_state_%1").arg(state - MLS_BOLD);
                    else
                        fntName = QString("disabled_state_%1").arg(state - MLS_BOLD);

                    ltype->EnableForcedFont(i, fntName);
                }

                if (i + skip == curItem)
                    ltype->SetItemCurrent(i);

            }

        }

        container->Draw(&tmp, 0, 0);
        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }

}


void ManagedList::cursorDown(bool page)
{
    if (curGroup)
    {
        int newIndex = curGroup->getCurIndex();
        int itemCount = curGroup->getItemCount();

        newIndex += (page ? listSize : 1);

        if (newIndex >= itemCount)
            newIndex = (page ? itemCount - 1 : newIndex - itemCount);

        while(curGroup->getItem(newIndex)->getEnabled() == false)
        {
            ++newIndex;
            if (newIndex >= itemCount )
                newIndex = 0;
        }

        curGroup->setCurIndex(newIndex);
        getParent()->update(listRect);
    }
}


void ManagedList::cursorUp(bool page)
{
    if (curGroup)
    {
        int newIndex = curGroup->getCurIndex();
        int itemCount = curGroup->getItemCount();

        newIndex -= (page ? listSize : 1);
        if (newIndex < 0)
            newIndex = (page ? 0 : itemCount + newIndex);

        while(curGroup->getItem(newIndex)->getEnabled() == false)
        {
            --newIndex;
            if (newIndex < 0 )
                newIndex = itemCount - 1;
        }

        curGroup->setCurIndex(newIndex);
        getParent()->update(listRect);
    }
}


void ManagedList::cursorLeft(bool page)
{
    curGroup->getCurItem()->cursorLeft(page);
}


void ManagedList::cursorRight(bool page)
{
    curGroup->getCurItem()->cursorRight(page);
}


void ManagedList::select()
{
    curGroup->getCurItem()->select();
}


void ManagedList::itemChanged(ManagedListItem* itm)
{
    if (itm)
        getParent()->update(listRect);
}


bool ManagedList::init(XMLParse *themeIn, const QString& containerNameIn, const QString& listNameIn, const QRect& r)
{
    UIListType *ltype = NULL;
    LayerSet *container = NULL;


    if (!themeIn || containerNameIn.isEmpty() || listNameIn.isEmpty())
    {
        cerr << "sanity check failed" << endl;
        return false;
    }

    theme = themeIn;
    containerName = containerNameIn;

    container = theme->GetSet(containerName);
    if (!container)
    {
        cerr << "Failed to get container " << containerName << endl;
        return false;
    }

    listName = listNameIn;

    ltype = (UIListType *)container->GetType(listName);
    if (!ltype)
    {
        cerr << "Failed to get list " << listName << endl;
        return false;
    }

    listSize = ltype->GetItems();
    listRect = r;

    return true;
}

void ManagedList::setCurGroup(ManagedListGroup* newGroup)
{
    newGroup->slotGuiActivate(newGroup);
    curGroup = newGroup;
    getParent()->update(listRect);
}

bool ManagedList::goBack()
{
    if (curGroup && curGroup->getParentGroup())
    {
        curGroup->doGoBack();
        return true;
    }

    return false;
}

