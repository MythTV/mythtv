/*
    mythcontainer.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object that holds UI widgets in a logical group.
	
*/

#include "mythcontainer.h"

MythUIContainer::MythUIContainer(const QString &name)
{
    m_name = name;
    m_area = QRect(0,0,0,0);

    /*
    
    m_context = -1;
    m_debug = false;
    numb_layers = -1;
    allTypes = new vector<UIType *>;
    
    */
}

MythUIContainer::~MythUIContainer()
{
/*
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
*/
}

/*
void LayerSet::AddType(UIType *type)
{
    type->SetDebug(m_debug);
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
    bumpUpLayers(type->getOrder());
}

UIType *LayerSet::GetType(const QString &name)
{
    UIType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void LayerSet::bumpUpLayers(int a_number)
{
    if (a_number > numb_layers)
    {
        numb_layers = a_number;
    }
}

void LayerSet::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        if (m_debug == true)
            cerr << "-LayerSet::Draw\n";
        UIType *type = (*i);
        type->Draw(dr, drawlayer, context);
    }
  }
}

void LayerSet::DrawRegion(QPainter *dr, QRect &area, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        if (m_debug == true)
            cerr << "-LayerSet::Draw\n";
        UIType *type = (*i);
        type->DrawRegion(dr, area, drawlayer, context);
    }
  }
}

void LayerSet::ClearAllText(void)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
        {
            QString defText = item->GetDefaultText();
            if ((defText == "" ) ||
                (defText.contains(QRegExp("%"))))
                item->SetText(QString(""));
        }
    }
}

void LayerSet::SetText(QMap<QString, QString> &infoMap)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
        {
            QMap<QString, QString>::Iterator riter = infoMap.begin();
            QString new_text = item->GetDefaultText();
            QString full_regex;

            if ((new_text == "") &&
                (infoMap.contains(item->Name())))
            {
                new_text = infoMap[item->Name()];
            }
            else if (new_text.contains(QRegExp("%.*%")))
            {
                for (; riter != infoMap.end(); riter++)
                {
                    QString key = riter.key().upper();
                    QString data = riter.data();

                    if (new_text.contains(key))
                    {
                        full_regex = QString("%") + key + QString("(\\|([^%|]*))?") +
                                     QString("(\\|([^%|]*))?") + QString("(\\|([^%]*))?%");
                        if (riter.data() != "")
                            new_text.replace(QRegExp(full_regex),
                                             QString("\\2") + data + QString("\\4"));
                        else
                            new_text.replace(QRegExp(full_regex), QString("\\6"));
                    }
                }
            }

            if (new_text != "")
                item->SetText(new_text);
        }
    }
}

void LayerSet::UseAlternateArea(bool useAlt)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *item = dynamic_cast<UITextType *>(type))
            item->UseAlternateArea(useAlt);
    }
}

void LayerSet::SetDrawFontShadow(bool state)
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        type->SetDrawFontShadow(state);
    }
}


*/

