#ifndef MANAGED_LIST_H
#define  MANAGED_LIST_H

#include <vector>
#include <qguardedptr.h>

using namespace std;

#include "uitypes.h"
#include "mythwidgets.h"
#include "settings.h"



enum ManagedListItemStates{MLS_NORMAL, MLS_BOLD, MLS_USER};

class ManagedList;
class ManagedListGroup;

///////////////////////////////////////////////////////////////////////////////
//
// A ManagedListItem represents, an item within a ManagedList (oddly enough).
//
// This is an abstract base class that defines the interface.
//
class MPUBLIC ManagedListItem : public QObject
{
    Q_OBJECT

    public:
        ManagedListItem(const QString& startingText = "", ManagedList* _parentList=NULL,
                        QObject* _parent=NULL, const char* _name=0);

        virtual bool hasLeft(){ return false; }
        virtual bool hasRight(){ return false; }

        const int getState() const { return curState; }
        void setState(int val) { curState = val; emit changed(this); }

        const bool getEnabled() const { return enabled; }
        virtual void setEnabled(bool val) { enabled = val; }

        ManagedList* getParentList() { return parentList; }
        virtual void setParentList(ManagedList* _parent);



        virtual void setValue(const QString& val) { valueText = val; syncTextToValue(); }
        //virtual void setValue(const char* val) { setValue(QString(val)); }
        virtual const QString& getValue() const { return valueText; }

        virtual const QString& getText() const { return text; }
        void setText(const QString& newText) { text = newText; emit changed(this); }

    public slots:
        virtual void cursorLeft(bool) { canceled(); }
        virtual void cursorRight(bool) { selected(); }
        virtual void select() { selected(); }
        virtual void gotFocus() {}
        virtual void slotGuiActivate(ManagedListGroup*) {};

    signals:
        void selected(ManagedListItem*);
        void changed(ManagedListItem*);
        void canceled(ManagedListItem*);

    protected:
        virtual void syncTextToValue() { changed(); }
        virtual void selected() { if (enabled) emit selected(this); }
        virtual void canceled() { if (enabled) emit canceled(this); }
        virtual void changed() { emit(changed(this)); }
        int curState;
        int listIndex;
        bool enabled;
        QGuardedPtr<ManagedList> parentList;

        QString text;
        QString valueText;
};


class MPUBLIC DialogDoneListItem : public ManagedListItem
{
    Q_OBJECT
    public:
        DialogDoneListItem(const QString& startingText, int _result, MythDialog* _dialog = NULL, ManagedList* _parentList=NULL,
                           QObject* _parent=NULL, const char* _name=0)
                         : ManagedListItem(startingText, _parentList, _parent, _name)
        {
            dialog = _dialog;
            resultValue = _result;
        }

        virtual bool hasLeft(){ return false; }
        virtual bool hasRight(){ return false; }

        virtual void selected() { if (enabled && dialog) dialog->done(resultValue); }

        void setDialog(MythDialog* dlg) { dialog = dlg;}

        MythDialog* getDialog() { return dialog;}

    protected:
        QGuardedPtr<MythDialog> dialog;
        int resultValue;
};



///////////////////////////////////////////////////////////////////////////////
//
// An integer value within a ManagedList.
//
// String templates are used to convert the value into a string. Defaults to "%1"
//
class IntegerManagedListItem : public ManagedListItem
{
    Q_OBJECT

    public:
        IntegerManagedListItem(int bigStepAmt = 10, int stepAmt=1,  ManagedList* parentList=NULL,
                               QObject* _parent=NULL, const char* _name=0);
        void setTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                          const QString& oneStr, const QString& posStr);
        void setShortTemplates(const QString& negStr, const QString& negOneStr,
                               const QString& zeroStr, const QString& oneStr,
                               const QString& posStr);

        int intValue() const { return valueText.toInt();}

        virtual void setValue(int newVal) { ManagedListItem::setValue(QString::number(newVal));}

        virtual void changeValue(int amount) { setValue(intValue() + amount); }

        const QString& getShortText() const { return shortText; }
        virtual bool hasLeft(){ return true; }
        virtual bool hasRight(){ return true; }
    public slots:
        virtual void cursorLeft(bool page = false) { if (enabled) changeValue(page ? (0 - bigStep) : (0 - step)); }
        virtual void cursorRight(bool page = false) { if (enabled) changeValue(page ? bigStep : step); }

    protected:
        virtual void syncTextToValue();
        void setText(const QString& newText) { text = newText; }
        int step;
        int bigStep;
        QString negTemplate;
        QString negOneTemplate;
        QString posTemplate;
        QString posOneTemplate;
        QString zeroTemplate;
        QString shortNegTemplate;
        QString shortNegOneTemplate;
        QString shortPosOneTemplate;
        QString shortPosTemplate;
        QString shortZeroTemplate;

        QString shortText;
};








///////////////////////////////////////////////////////////////////////////////
//
// A list item that can contain more items.
//
class MPUBLIC ManagedListGroup : public ManagedListItem
{
    Q_OBJECT

    public:
        ManagedListGroup(const QString& txt, ManagedListGroup* pGroup, ManagedList* parentList=NULL,
                         QObject* _parent=NULL, const char* _name=0);

        const QPtrList<ManagedListItem>* getItems() const { return &itemList;}
        bool addItem(ManagedListItem* item, int where = -1);

        const int getItemCount() const { return itemCount;}

        virtual bool hasLeft(){ return false; }
        virtual bool hasRight(){ return (itemCount > 0); }

        ManagedListGroup* getParentGroup() { return parentGroup; }
        void setParentGroup(ManagedListGroup* pGroup) { parentGroup = pGroup; }


        virtual void cursorRight(bool page = false);
        virtual void select();

        int getCurIndex() const { return curItem; }
        void setCurIndex(int newVal);

        ManagedListItem* getItem(int index) { return itemList.at(index); }
        ManagedListItem* getCurItem() { return itemList.at(curItem); }

        const QString getCurItemValue() { return getItemValue(curItem); }
        const QString getItemValue(int which) { ManagedListItem *itm = getItem(which);
                                                return itm ? itm->getValue() : 0; }

        const QString getCurItemText() { return getItemText(curItem); }
        const QString getItemText(int which) { ManagedListItem *itm = getItem(which);
                                               return itm ? itm->getText() : ""; }

        virtual void setParentList(ManagedList* _parent);

        virtual void clear();

    public slots:
        virtual void doGoBack();
        virtual void slotGuiActivate(ManagedListGroup*);
    signals:
        void goingBack();
        void wentBack();

    protected:
        QPtrList<ManagedListItem> itemList;
        int curItem;
        int itemCount;
        QGuardedPtr<ManagedListGroup> parentGroup;
        QGuardedPtr<ManagedListItem> goBack;
};


///////////////////////////////////////////////////////////////////////////////
//
// A specialized version of the ManagedListGroup that allows for swtiching the
// values using left/right without having to pop up the list.
//
class MPUBLIC SelectManagedListItem : public ManagedListGroup
{
    Q_OBJECT

    public:
        SelectManagedListItem(const QString& baseText, ManagedListGroup* pGroup, ManagedList* parentList,
                              QObject* _parent=NULL, const char* _name=0);
        virtual ManagedListItem* addSelection(const QString& label, QString value=QString::null, bool selectit=false);
        virtual ManagedListItem* addButton(const QString& label, QString value=QString::null, bool selectit=false);

        virtual void clearSelections(void);

        virtual void cursorRight(bool page = false);
        virtual void cursorLeft(bool page = false);
        virtual void select();

        virtual void selectValue(const QString& newValue) { select(newValue, true);}
        virtual void setValue(const QString& newValue);
        virtual void selectText(const QString& newValue) { select(newValue, false);}
        virtual void selectItem(int index) { curItem = index; changed(); }
        virtual void select(const QString& newValue, bool bValue);

        virtual bool hasLeft(){ return true; }
        virtual bool hasRight(){ return true; }


        virtual int getValueIndex(QString value)  {
           int i = -1;
           for(ManagedListItem* tempItem = itemList.first(); tempItem; tempItem = itemList.next() )
            {
                i++;
                if (tempItem->getValue() == value)
                    return i;
            }
            return -1;
        };

        virtual int getTextIndex(QString txt)  {
           int i = -1;
           for(ManagedListItem* tempItem = itemList.first(); tempItem; tempItem = itemList.next() )
            {
                i++;
                if (tempItem->getText() == txt)
                    return i;
            }

            return -1;
        };

    signals:
        void selectionAdded(const QString& label, QString value, bool select = false);
        void selectionsCleared(void);
        void buttonPressed(ManagedListItem* itm, ManagedListItem* button);

    public slots:
        virtual void buttonSelected(ManagedListItem* itm);
        virtual void itemSelected(ManagedListItem* itm);
        virtual void doGoBack();

    protected:
        bool isSet;
        QString baseText;
        int lastItem;
};


class MPUBLIC BoolManagedListItem : public SelectManagedListItem
{
    Q_OBJECT
    public:
        BoolManagedListItem(bool initialValue, ManagedListGroup* pGroup=NULL, ManagedList* parentList=NULL,
                            QObject* _parent=NULL, const char* _name=0);
        void setLabels(const QString& trueLbl, const QString& falseLbl);
        virtual bool hasLeft(){ return true; }
        virtual bool hasRight(){ return true; }


        virtual void setValue(bool val)
        {
            // DS note: using QString::number(val) blows up...
            if (val)
                SelectManagedListItem::setValue("1");
            else
                SelectManagedListItem::setValue("0");
        }


        bool boolValue() const {return (bool)valueText.toInt();}



    public slots:
        virtual void cursorLeft(bool) { if (enabled) setValue(!boolValue()); }
        virtual void cursorRight(bool) { if (enabled) setValue(!boolValue()); }
        virtual void slotGuiActivate(ManagedListGroup*) { generateList(); }

    protected:
        void generateList();
        void setText(const QString& newText) { text = newText; emit(changed(this)); }
        QString trueLabel;
        QString falseLabel;
        bool initialValue;
        bool listBuilt;
};



///////////////////////////////////////////////////////////////////////////////
//
// An IntegerManagedListItem with a minimum and maximum value.
//
class MPUBLIC BoundedIntegerManagedListItem : public SelectManagedListItem
{
    Q_OBJECT

    public:
        BoundedIntegerManagedListItem(int minValIn, int maxValIn, int _bigStep, int _step = 1,
                                      ManagedListGroup* _group=NULL, ManagedList* parentList=NULL,
                                      QObject* _parent=NULL, const char* _name=0, bool _invert = false);


        virtual bool hasLeft()
        {
            if (inverted)
                return intValue() < maxVal;
            else
                return intValue() > minVal;
        }

        virtual bool hasRight()
        {
            if (inverted)
                return intValue() > minVal;
            else
                return intValue() < maxVal;
        }

        void setTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                          const QString& oneStr, const QString& posStr);

        int intValue() const { return valueText.toInt();}

        virtual void setValue(int newVal);

        virtual void changeValue(int amount) { setValue(intValue() + amount); }


        QString numericToString(int v);


    public slots:
        virtual void cursorLeft(bool page = false)
        {
            if (enabled)
            {
                if (inverted)
                    changeValue(page ? bigStep : step);
                else
                    changeValue(page ? (0 - bigStep) : (0 - step));
            }
        }

        virtual void cursorRight(bool page = false)
        {
            if (enabled)
            {
                if (inverted)
                    changeValue(page ? (0 - bigStep) : (0 - step));
                else
                    changeValue(page ? bigStep : step);
            }
        }


        virtual void slotGuiActivate(ManagedListGroup*) { generateList(); }

    protected:
        void generateList();
        int step;
        int bigStep;
        QString negTemplate;
        QString negOneTemplate;
        QString posTemplate;
        QString posOneTemplate;
        QString zeroTemplate;

    protected:
        int maxVal;
        int minVal;
        bool listBuilt;
        bool inverted;
};


class MPUBLIC ManagedListSetting: public SimpleDBStorage
{
    Q_OBJECT

    protected:
        ManagedListSetting(QString _table, QString _column, ManagedList* _parentList=NULL):
            SimpleDBStorage(_table, _column ),
            parentList(_parentList) {
            listItem = NULL;
        };

        QGuardedPtr<ManagedList> parentList;
        QGuardedPtr<ManagedListItem> listItem;

    public slots:
        void itemChanged(ManagedListItem*) { syncDBFromItem(); }



    public:

        virtual void load() {
            SimpleDBStorage::load();
            syncItemFromDB();
        }

        virtual void setValue(int val) {setValue(QString::number(val));}

        virtual void setValue(const QString& val) {
            if (listItem)
            {
                listItem->setValue(val);
                syncDBFromItem();
            }
            else
            {
                SimpleDBStorage::setValue(val);
            }
        }

        virtual const QString getValue() {
            if (listItem)
            {
                syncDBFromItem();
                return listItem->getValue();
            }
            else
            {
                return SimpleDBStorage::getValue();
            }

        }

        virtual void syncDBFromItem()
        {
            if (listItem)
                SimpleDBStorage::setValue(listItem->getValue());
        }

        virtual void syncItemFromDB()
        {
            if (listItem)
                listItem->setValue(settingValue);
        }

        ManagedListItem* getItem() { return listItem; }
        operator ManagedListItem*() { return listItem; }
};


class MPUBLIC SelectManagedListSetting : public ManagedListSetting
{
    protected:
        SelectManagedListSetting(const QString& listName,  const QString& listText, ManagedListGroup* _group,
                                 QString _table, QString _column, ManagedList* _parentList=NULL)
                               : ManagedListSetting(_table, _column, _parentList)
        {
            constructListItem(listText, _group, _parentList, listName);
            listItem = selectItem;

            connect(listItem, SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
        }

        QGuardedPtr<SelectManagedListItem> selectItem;

        virtual void constructListItem(const QString& listText, ManagedListGroup* _group,
                                                         ManagedList* _parentList, const QString& listName)
        {
            selectItem = new SelectManagedListItem(listText, _group, _parentList, this, listName);
        }

    public:
        ManagedListItem* addSelection(const QString& label, const QString& value)
        {
            if (selectItem)
                return selectItem->addSelection(label, value);

            return NULL;
        }

        ManagedListItem* addButton(const QString& label, const QString& value)
        {
            if (selectItem)
                return selectItem->addButton(label, value);

            return NULL;
        }

        ManagedListItem* addSelection(const QString& label, int value)
        {
            if (selectItem)
                return selectItem->addSelection(label, QString::number(value));

            return NULL;
        }

        void clearSelections() {
            if (selectItem)
                selectItem->clearSelections();
        }

        operator SelectManagedListItem* () { return selectItem; }
};


class MPUBLIC BoolManagedListSetting : public ManagedListSetting
{
    public:
        BoolManagedListSetting(const QString& trueText, const QString& falseText, const QString& ItemName,
                               QString _table, QString _column, ManagedListGroup* _group,
                               ManagedList* _parentList=NULL)
                            : ManagedListSetting(_table, _column, _parentList)
        {
            boolListItem = new BoolManagedListItem(false, _group, _parentList, this, ItemName);
            listItem = boolListItem;
            boolListItem->setLabels(trueText, falseText);
            connect(listItem, SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
        }

        operator BoolManagedListItem* () { return boolListItem; }

    protected:
        BoolManagedListItem* boolListItem;
};


class MPUBLIC IntegerManagedListSetting : public ManagedListSetting
{
    public:
        IntegerManagedListSetting(int _bigStep, int _step, const QString& ItemName, QString _table,
                                  QString _column, ManagedList* _parentList=NULL)
                            : ManagedListSetting(_table, _column, _parentList)
        {
            IntegerListItem = new IntegerManagedListItem(_bigStep, _step, _parentList, this, ItemName);
            listItem = IntegerListItem;
            connect(listItem, SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
        }

        operator IntegerManagedListItem* () { return IntegerListItem; }

        void setTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                          const QString& oneStr, const QString& posStr)
        {
            IntegerListItem->setTemplates(negStr, negOneStr, zeroStr, oneStr, posStr);
        }

        void setShortTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                               const QString& oneStr, const QString& posStr)
        {
            IntegerListItem->setShortTemplates(negStr, negOneStr, zeroStr, oneStr, posStr);
        }
    protected:
        IntegerManagedListItem* IntegerListItem;
};

class MPUBLIC BoundedIntegerManagedListSetting : public ManagedListSetting
{
    public:
        BoundedIntegerManagedListSetting(int _min, int _max, int _bigStep, int _step, const QString& ItemName,
                                  QString _table, QString _column, ManagedListGroup* _group,
                                  ManagedList* _parentList=NULL, bool _invert = false)
                            : ManagedListSetting(_table, _column, _parentList)
        {
            BoundedIntegerListItem = new BoundedIntegerManagedListItem(_min, _max, _bigStep, _step,
                                                                       _group, _parentList, this, ItemName,
                                                                       _invert);
            listItem = BoundedIntegerListItem;
            connect(listItem, SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
        }

        operator BoundedIntegerManagedListItem* () { return BoundedIntegerListItem; }

        void setTemplates(const QString& negStr, const QString& negOneStr, const QString& zeroStr,
                          const QString& oneStr, const QString& posStr)
        {
            BoundedIntegerListItem->setTemplates(negStr, negOneStr, zeroStr, oneStr, posStr);
        }

    protected:
        BoundedIntegerManagedListItem* BoundedIntegerListItem;
};



///////////////////////////////////////////////////////////////////////////////
//
// The ManagedList takes care of the heavy lifting when using a UIListType.
//
class MPUBLIC ManagedList : public QObject
{
    Q_OBJECT

    public:
        ManagedList(MythDialog* parent, const char* name=0);

        void paintEvent(const QRect& r, QPainter *p, bool force=NULL);
        void update(QPainter *p);

        const XMLParse* getTheme() const { return theme; }
        void setTheme(XMLParse* newTheme) { theme = newTheme; }

        const QString& getContainerName() const { return containerName; }
        void setContainerName(const QString& newStr) { containerName = newStr; }


        ManagedListItem* getItem(const QString& itemName) { return (ManagedListItem*)child(itemName);}
        MythDialog* getParent() { return (MythDialog*)parent();}

        bool init(XMLParse *theme, const QString& containerNameIn, const QString& listNameIn, const QRect& r);

        ManagedListGroup* getCurGroup() { return curGroup; }
        void setCurGroup(ManagedListGroup* newGroup);

        bool goBack();
        bool getLocked() const { return locked; }
        void setLocked(bool val = true) { locked = val; }
        
        

    public slots:
        void cursorDown(bool page = false);
        void cursorUp(bool page = false);
        void cursorLeft(bool page = false);
        void cursorRight(bool page = false);
        void select();
        void itemChanged(ManagedListItem* itm);
        //void itemCanceled(ManagedListItem* itm);
        //void itemSelected(ManagedListItem* itm);
        

    protected:
        QGuardedPtr<ManagedListGroup> curGroup;
        XMLParse* theme;
        int listSize;
        QString containerName;
        QString listName;
        QRect listRect;
        bool locked;
};



#endif
