#ifndef MTCPINPUT_H
#define MTCPINPUT_H
/*
	mtcpinput.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mtcp input object

*/

#include <stdint.h>
#include <vector>
using namespace std;

#include <qvaluevector.h>


class MtcpInput
{

  public:

    MtcpInput(std::vector<char> *raw_data);
    MtcpInput(QValueVector<char> *raw_data);

    ~MtcpInput();

    char        peekAtNextCode();
    char        popGroup(QValueVector<char> *group_contents);
    char        popByte();
    uint16_t    popU16();
    uint32_t    popU32();
    uint        amountLeft();
    uint        size(){return amountLeft();}

    QString     popName();
    void        popProtocol(int *major, int *minor);   
    uint32_t    popJobCount();
    
    QString     popMajorDescription();
    QString     popMinorDescription();
    
    uint32_t    popMajorProgress();
    uint32_t    popMinorProgress();

/*
    int         popStatus();
    uint32_t    popSessionId();
    uint32_t    popCollectionCount();
    uint32_t    popCollectionId();
    uint32_t    popCollectionType();
    bool        popCollectionEditable();
    bool        popCollectionRipable();
    bool        popCollectionBeingRipped();
    uint32_t    popCollectionGeneration();
    bool        popUpdateType();
    uint32_t    popTotalItems();
    uint32_t    popAddedItems();
    uint32_t    popDeletedItems();
    uint8_t     popItemType();
    uint32_t    popItemId();
    uint8_t     popItemRating();
    uint32_t    popItemLastPlayed();
    uint32_t    popItemPlayCount();
    uint32_t    popItemYear();
    uint32_t    popItemTrack();
    uint32_t    popItemLength();
    bool        popItemDupFlag();
    QString     popItemUrl();
    QString     popItemArtist();
    QString     popItemAlbum();
    QString     popItemTitle();
    QString     popItemGenre();
    int         popListId();
    QString     popListName();
    uint32_t    popListItem();
    uint32_t    popDeletedList();
    bool        popListEditable();
    bool        popListRipable();
    bool        popListBeingRipped();
    uint32_t    popDeletedItem();
    QString     popListItemName();
    bool        popCommitListType();
*/

    void        printContents();    // Debugging
    
    
  private:

    QValueVector<char> contents;
    int pos;
};

#endif
