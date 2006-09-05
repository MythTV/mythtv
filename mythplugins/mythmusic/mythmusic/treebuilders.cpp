#include <mythtv/mythcontext.h>
#include "treebuilders.h"

typedef struct {
    QString field;
    MetadataPtrList list;
} Branch;

typedef struct  {
   QString testStr;
   QString dispStr;
} FieldSplitInfo;

static FieldSplitInfo splitArray4[] =
{ 
  {"!\"#$%&'()*+,-./:;<=>?@[\\]^_{|}~", " (...)"},
  {"01234", " (0 1 2 3 4)" },
  {"56789", " (5 6 7 8 9)" },
  {"ABCDE", " (A B C D E)"},
  {"FGHIJ", " (F G H I J)"},
  {"KLMNO", " (K L M N O)"},
  {"PQRST", " (P Q R S T)"},
  {"UVWXYZ", " (U V W X Y Z)"}
};
const int kSplitArray4_Max = sizeof splitArray4 / sizeof splitArray4[0];

static QString thePrefix = "the ";

MusicTreeBuilder::MusicTreeBuilder() 
{
    m_depth = -1;
}

MusicTreeBuilder::~MusicTreeBuilder() 
{
}

void MusicTreeBuilder::makeTree(MusicNode *root, const MetadataPtrList &metas) 
{
    m_depth++;
        
    typedef QMap<QString, Branch*> BranchMap;
    BranchMap branches;
    
    Metadata *meta;
    QPtrListIterator<Metadata> iter(metas);
    while ((meta = iter.current()) != 0) 
    {
        if (isLeafDone(meta)) 
        {
            root->addLeaf(meta);
        } 
        else 
        {
            QString field = getField(meta);
            QString field_key = field.lower();

            if (field_key.left(4) == thePrefix) 
                field_key = field_key.mid(4);

            Branch *branch = branches[field_key];
            if (branch == NULL) 
            {
                branch = new Branch;
                branch->field = field;
                branches[field_key] = branch;
            }
            branch->list.append(meta);
        }

        ++iter;
    }

    for(BranchMap::iterator it = branches.begin(); it != branches.end(); it++) 
    {
        Branch *branch = it.data();
        MusicNode *sub_node = createNode(branch->field);
        root->addChild(sub_node);
        makeTree(sub_node, branch->list);
        delete branch;
    }

    m_depth--;
}

class MusicFieldTreeBuilder : public MusicTreeBuilder 
{
  public:
    MusicFieldTreeBuilder(const QString &paths) 
    {
        m_paths = QStringList::split(' ', paths);
    }

    ~MusicFieldTreeBuilder() 
    {
    }
    
    void makeTree(MusicNode *root, const MetadataPtrList &metas) 
    {
        if ((uint)getDepth() + 2 >= m_paths.size()) 
        {
            root->setLeaves(metas);
            return;
        }

        MusicTreeBuilder::makeTree(root, metas);
    }
    
protected:
    MusicNode *createNode(const QString &title) 
    {
        return new MusicNode(title, m_paths[getDepth()]);
    }

    bool isLeafDone(Metadata *) 
    {
        return false;
    }
    
    QString getField(Metadata *meta) 
    {
        QString field = m_paths[getDepth()];
        
        if (field == "splitartist1" || 
            field == "splitartist") 
        {
            return getSplitField(meta, field);
        } 

        QString data;
        meta->getField(field, &data);
        return data;
    }

private:
    QStringList m_paths;
    QMap<QChar, QString> m_split_map;

    QString getSplitField(Metadata *meta, const QString &field) 
    {
        QString firstchar_str = meta->FormatArtist().stripWhiteSpace();

        if (firstchar_str.left(4).lower() == thePrefix) 
            firstchar_str = firstchar_str.mid(4,1).upper();
        else 
            firstchar_str = firstchar_str.left(1).upper();
        
        QChar firstchar = firstchar_str[0];
        QString split = m_split_map[firstchar];
        
        if (split.isEmpty()) 
        {
            if (field == "splitartist1") 
            {
                split = QObject::tr("Artists") + " (" + firstchar + ")";
                m_split_map[firstchar] = split;
            } 
            else 
            {
                int split_max = kSplitArray4_Max;
                FieldSplitInfo *splits = splitArray4;            
            
                for(int i = 0; i < split_max; i++) 
                {
                    if (splits[i].testStr.contains(firstchar)) 
                    {
                        split = QObject::tr("Artists") + splits[i].dispStr;
                        m_split_map[firstchar] = split;
                        break;
                    }
                }
            }
        }

        if (split.isEmpty()) 
        {
            split = QObject::tr("Artists") + " (" + firstchar + ")";
            m_split_map[firstchar] = split;
        }

        return split;
    }
};

class MusicDirectoryTreeBuilder : public MusicTreeBuilder 
{
  public:
    MusicDirectoryTreeBuilder() 
    {
        m_startdir = gContext->GetSetting("MusicLocation");
    }

    ~MusicDirectoryTreeBuilder() 
    {
        for(MetaMap::iterator it = m_map.begin(); it != m_map.end(); it++)
            delete it.data();
    }

protected:
    MusicNode *createNode(const QString &title) 
    {
        return new MusicNode(title, "directory");
    }

    bool isLeafDone(Metadata *meta) 
    {
        return(uint)getDepth() + 1 >= getPathsForMeta(meta)->size();
    }

    QString getField(Metadata *meta) 
    {
        return getPathsForMeta(meta)->operator[](getDepth());
    }

  private:
    inline QString getStartdir(void) { return m_startdir; }

    QStringList* getPathsForMeta(Metadata *meta) 
    {
        QStringList *paths = m_map[meta];

        if (paths)
            return paths;

        QString filename = meta->Filename().remove(0, getStartdir().length());
        paths = new QStringList(QStringList::split('/', filename));
        m_map[meta] = paths;
        
        return paths;
    }

    typedef QMap<Metadata*,QStringList*> MetaMap;
    MetaMap m_map;
    QString m_startdir;

};

MusicTreeBuilder *MusicTreeBuilder::createBuilder(const QString &paths) 
{
    if (paths == "directory")
        return new MusicDirectoryTreeBuilder();

    return new MusicFieldTreeBuilder(paths);
}

