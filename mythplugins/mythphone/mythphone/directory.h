/*
	directory.h

	(c) 2004 Paul Volkaerts
	
  Directory class holding a simple contact database plus call history.
*/
#ifndef DIRECTORY_H_
#define DIRECTORY_H_

#include <qvaluelist.h>
#include <qlistview.h>
#include <qptrlist.h>
#include <qsqldatabase.h>
#include <qthread.h>

#include <mythtv/uitypes.h>



// Values for the Attribute[0] on the Managed Tree
#define TA_ROOT           0
#define TA_DIR            1
#define TA_DIRENTRY       2
#define TA_VMAIL          3
#define TA_VMAIL_ENTRY    4
#define TA_CALLHISTENTRY  5
#define TA_SPEEDDIALENTRY 6


class DirEntry
{

  public:

        DirEntry(QString nn, QString uri, QString fn="", QString sn="", QString ph="", bool ohl=false);
        DirEntry(DirEntry *Original);
        ~DirEntry();
        QString getNickName() { return NickName; }
        QString getSurname() { return Surname; }
        QString getFirstName() { return FirstName; }
        QString getFullName() { if (FirstName.length() != 0) return FirstName + " " + Surname; else return Surname; }
        QString getPhotoFile() { return PhotoFile; }
        QString getUri() { return Uri; }
        bool    getOnHomeLan() { return onHomeLan; }
        void    setNickName(QString s) { NickName=s; changed=true; }
        void    setSurname(QString s) { Surname=s; changed=true; }
        void    setFirstName(QString s) { FirstName=s; changed=true; }
        void    setPhotoFile(QString s) { PhotoFile=s; changed=true; }
        void    setOnHomeLan(bool b) { onHomeLan=b; changed=true; }
        void    setUri(QString s) { Uri=s; changed=true; }
        int     getId() { return id; }
        void    setDbId(int d) { dbId=d; }
        int     getDbId() { return dbId; }
        void    writeTree(GenericTree *tree_to_write_to, GenericTree *sdTree=0);
        void    setSpeedDial(bool yn) { SpeedDial = yn; changed=true; }
        bool    isSpeedDial() { return SpeedDial; }
        bool    urlMatches(QString s);
        void    setDBUpToDate() { changed=false; inDatabase=true; }
        void    updateYourselfInDB(QSqlDatabase *db, QString Dir);
        void    deleteYourselfFromDB(QSqlDatabase *db);


  private:

        QString NickName;
        QString FirstName;
        QString Surname;
        QString Uri;
        QString PhotoFile;
        int     id;
        bool    SpeedDial;
        bool    onHomeLan;

        bool    inDatabase;
        bool    changed;
        int     dbId;

	// To Add:-
	//	call options for this entry
	//	video/voice capabilities/default
};

class Directory : public QPtrList<DirEntry>
{
  public:
    
    Directory(QString Name);
    ~Directory();
    QString getName() { return name; }
    void writeTree(GenericTree *tree_to_write_to, GenericTree *sdTree);
    DirEntry *fetchById(int id);
    void getMatchingCalls(DirEntry *source, QPtrList<DirEntry> &CallList);
    virtual int compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2);
    DirEntry *getDirEntrybyDbId(int dbId);
    DirEntry *getDirEntrybyUrl(QString Url);
    void saveChangesinDB(QSqlDatabase *db);
    void deleteEntry(QSqlDatabase *db, DirEntry *Entry);
    void AddAllEntriesToList(QStrList &l, bool SpeeddialsOnly);

  private:
    QString name;
};


class CallRecord
{

  public:

    CallRecord(QString dn, QString uri, bool callIn, QString ts);
    CallRecord(CallRecord *Original);
    CallRecord(DirEntry *Original, bool callIn, QString ts);
    ~CallRecord();
    QString getTimestamp() { return timestamp; }
    QString getDisplayName() { return DisplayName; }
    QString getUri() { return Uri; }
    int     getId() { return id; }
    void    setDuration(int d) { Duration = d; }
    int     getDuration() { return Duration; }
    void    setDbId(int d) { dbId=d; }
    void    setDBUpToDate() { changed=false; inDatabase=true; }
    bool    isIncoming() { return DirectionIn; }
    void    writeTree(GenericTree *tree_to_write_to);
    void    updateYourselfInDB(QSqlDatabase *db);
    void    deleteYourselfFromDB(QSqlDatabase *db);

  private:

    QString DisplayName;
    QString Uri;
    int id;
    QString timestamp;	// Used for missed/placed calls
    int Duration;		// Call Duration or 0 for unanswered
    bool DirectionIn;	// Call Direction, in or out

    bool    inDatabase;
    bool    changed;
    int     dbId;
};


class CallHistory : public QPtrList<CallRecord>
{
  public:

    	CallHistory() : QPtrList<CallRecord>() {};
    	~CallHistory();
        void writeTree(GenericTree *placed_tree, GenericTree *received_tree);
        CallRecord *fetchById(int id);
        virtual int compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2);
        void saveChangesinDB(QSqlDatabase *db);
        void deleteRecords(QSqlDatabase *db);
  private:
};



class DirectoryContainer
{
  public:
  
    DirectoryContainer(QSqlDatabase *db_ptr);
    ~DirectoryContainer();
    void Load();
    void writeTree();
    DirEntry *fetchDirEntryById(int id);
    CallRecord *fetchCallRecordById(int id);
    void AddEntry(DirEntry *entry, QString Dir, bool addToUITree);
    void ChangeEntry(DirEntry *entry, QString nn, QString Url, QString fn, QString sn, QString ph, bool OnHomeLan);
    QStrList getDirectoryList(void);
    GenericTree *addToTree(QString DirName);
    void getRecentCalls(DirEntry *source, CallHistory &RecentCalls);
    void AddToCallHistory(CallRecord *entry, bool addToUITree);
    void clearCallHistory();
    DirEntry *getDirEntrybyDbId(int dbId);
    void saveChangesinDB(QSqlDatabase *db);
    void addToTree(DirEntry *newEntry, QString Dir);
    void deleteFromTree(GenericTree *treeObject, DirEntry *entry);
    void createTree();
    GenericTree *getTreeRoot() { return TreeRoot; }
    void setSpeedDial(DirEntry *entry);
    void removeSpeedDial(DirEntry *entry);
    void clearAllVoicemail();
    void deleteVoicemail(QString vmailName);
    DirEntry *FindMatchingDirectoryEntry(QString url);
    QStrList ListAllEntries(bool SpeeddialsOnly);


  private:
    Directory *fetch(QString Dir);
    GenericTree *findInTree(GenericTree *Root, int at1, int atv1, int at2, int atv2);
    void PutVoicemailInTree(GenericTree *tree_to_write_to);

    QPtrList<Directory>  AllDirs;
    CallHistory         *callHistory;

    GenericTree         *TreeRoot,
                        *voicemailTree,
                        *receivedcallsTree,
                        *placedcallsTree,
                        *speeddialTree;               
    QSqlDatabase        *sqlDB;
};




#endif
