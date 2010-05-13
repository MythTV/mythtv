#include <mythcontext.h>
#include "treebuilders.h"

typedef struct {
    QString field;
    MetadataPtrList list;
} Branch;

typedef struct  {
   QString testStr;
   QString dispStr;
} FieldSplitInfo;

// arrays for different level of granularity in the tree
// choose between them by using a global setting.  In an ideal
// world we would generate these dynamically, but that turned
// into more change than it was worth
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

static FieldSplitInfo splitArray16[] =
{ 
  {"!\"#$%&'()*+,-./:;<=>?@[\\]^_{|}~", " (...)"},
  {"01234", " (0 1 2 3 4)" },
  {"56789", " (5 6 7 8 9)" },
  {"AB", " (A B)"},
  {"CD", " (C D)"},
  {"EF", " (E F)"},
  {"GH", " (G H)"},
  {"IJ", " (I J)"},
  {"KL", " (K L)"},
  {"MN", " (M N)"},
  {"OP", " (O P)"},
  {"QR", " (Q R)"},
  {"ST", " (S T)"},
  {"UV", " (U V)"},
  {"WX", " (W X)"},
  {"YZ", " (Y Z)"}
};

static FieldSplitInfo splitArray29[] =
{ 
  {"!\"#$%&'()*+,-./:;<=>?@[\\]^_{|}~", " (...)"},
  {"01234", " (0 1 2 3 4)" },
  {"56789", " (5 6 7 8 9)" },
  {"A", " A"},
  {"B", " B"},
  {"C", " C"},
  {"D", " D"},
  {"E", " E"},
  {"F", " F"},
  {"G", " G"},
  {"H", " H"},
  {"I", " I"},
  {"J", " J"},
  {"K", " K"},
  {"L", " L"},
  {"M", " M"},
  {"N", " N"},
  {"O", " O"},
  {"P", " P"},
  {"Q", " Q"},
  {"R", " R"},
  {"S", " S"},
  {"T", " T"},
  {"U", " U"},
  {"V", " V"},
  {"W", " W"},
  {"X", " X"},
  {"Y", " Y"},
  {"Z", " Z"}
};

const int kSplitArray4_Max = sizeof splitArray4 / sizeof splitArray4[0];
const int kSplitArray16_Max = sizeof splitArray16 / sizeof splitArray16[0];
const int kSplitArray29_Max = sizeof splitArray29 / sizeof splitArray29[0];

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
    
    MetadataPtrList::const_iterator it = metas.begin();
    for (; it != metas.end(); ++it)
    {
        Metadata *meta = *it;
        if (isLeafDone(meta)) 
        {
            root->addLeaf(meta);
        } 
        else 
        {
            QString field = getField(meta);
            QString field_key = field.toLower();

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
    }

    for(BranchMap::iterator it = branches.begin(); it != branches.end(); it++) 
    {
        Branch *branch = *it;
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
        m_paths = paths.split(' ', QString::SkipEmptyParts);
    }

    ~MusicFieldTreeBuilder() 
    {
    }
    
    void makeTree(MusicNode *root, const MetadataPtrList &metas) 
    {
        if (getDepth() + 2 >= m_paths.size()) 
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
        QString firstchar_str = meta->FormatArtist().trimmed();

        if (firstchar_str.left(4).toLower() == thePrefix) 
            firstchar_str = firstchar_str.mid(4,1).toUpper();
        else 
            firstchar_str = firstchar_str.left(1).toUpper();
        
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
                QString artistGrouping = gCoreContext->GetSetting("ArtistTreeGroups", "none");
                if (artistGrouping == "2") 
                {
                    int split_max = kSplitArray29_Max;
                    FieldSplitInfo *splits = splitArray29;            
                
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
                else
                {
                    if (artistGrouping == "1")
                    {
                        int split_max = kSplitArray16_Max;
                        FieldSplitInfo *splits = splitArray16;            
                    
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
                    else
                    {
                        // old behaviour is the default
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
        m_startdir = gCoreContext->GetSetting("MusicLocation");
    }

    ~MusicDirectoryTreeBuilder() 
    {
        for(MetaMap::iterator it = m_map.begin(); it != m_map.end(); it++)
            delete *it;
    }

protected:
    MusicNode *createNode(const QString &title) 
    {
        return new MusicNode(title, "directory");
    }

    bool isLeafDone(Metadata *meta) 
    {
        return getDepth() + 1 >= getPathsForMeta(meta)->size();
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
        paths = new QStringList(filename.split('/'));
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

