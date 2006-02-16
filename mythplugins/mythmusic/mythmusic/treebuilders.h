#ifndef TREEBUILDERS_H_
#define TREEBUILDERS_H_

#include <qstring.h>
#include <qstringlist.h>
#include <qptrdict.h>
#include <qdict.h>

#include "metadata.h"

/** \class MusicTreeBuilder
    \brief superclass for the objects that build trees of \p Metadata
    
    This is the interface for objects that builds trees of \p Metadata
    objects into \p MusicNode objects.

    The basic idea of operation is, that the superclass provices a
    makeTree method that depends on the subclasses for a few methods
    to determine which fields of the metadata objects are used in
    building the tree and where the metadata objects go.

    This means that the superclass is fairly generic and does not
    itself operate on or query the metadata objects, but leaves that
    to the subclass. It's only responsibility is to determine the
    branches that go into the tree and then add metadata objects as
    leafs to the appropriate nodes.

    Ie. if you wanted to build the MythMusic tree based on the size of
    the audio files by implementing a \p MusicSizeTreeBuilder, the
    approach would be to get \p MusicSizeTreeBuilder::getField return
    the number of MB or KB blocks depending on the current depth as
    obtained by \p getDept. Then you could either overload \p makeTree
    to stop at a certain depth (like \p MusicFieldBuilder does) or
    make \p MusicSizeTreeBuilder::isLeafDone return true if you've
    reached the desired granularity.

    \note This method is fairly generic, and uses very little of the
    interfaces of \p MusicNode and \p Metadata, so it may someday be
    made more generic and of use to other plugins that needs trees.

    \note Calling this repeated using the same or new instance on the
    same root node (ie. to progressively populate it) hasn't been
    tested.
 */
class MusicTreeBuilder 
{
  public:
    virtual ~MusicTreeBuilder();

    /** \fn Create a tree using the list of \p Metadata objects and add them to the given root.

        This method will recurse and operate of the list the metadata
        objects in \p metas. It's implementation may and probably will
        differ depending on the subclass of builder you've obtained,
        depending on the kind of tree it builds.

        It relies heavily on subclasses implementing \p
        MusicTreeBuilder::isLeafDone and \p MusicTreeBuilder::getField
        for operation, and for performance, the subclasses should
        cache computed data in these methods as efficiently as possible.
     */
    virtual void makeTree(MusicNode *root, const MetadataPtrList &metas);

    /** \fn Create an \p MusicTreeBuilder for the appropriate path.

        Returns a \p MusicTreeBuilder subclass for building directory
        based or "artist album" field based trees.
     */
    static MusicTreeBuilder *createBuilder(const QString &paths);

  protected:
    MusicTreeBuilder();

    /** \fn Allocates and returns a new \p MusicNode.

        Implemented by the subclass. This method should allocate and a
        return a \p MusicNode with the approriate "level" set.
     */
    virtual MusicNode *createNode(const QString &title) = 0;

    /** \fn Determine is a \p Metadata should be track at the current depth.
 
        Ie. the directory builder will return true if the given \p
        Metadata's path at the current depth is the filename.

         Gets called repeatedly from \p MusicTreeBuilder::makeTree
         during tree creation and should only get called once
         pr. depth pr. \p Metadata.
     */
    virtual bool isLeafDone(Metadata *m) = 0;

    /** \fn Get the field value for the given \p Metadata at the current depth.

         Ie. the field builder will call \p Metadata::getField with
         the appropriate field name for the current dept.

         Gets called repeatedly from \p MusicTreeBuilder::makeTree
         during tree creation and may get called multiple times at the
         same depth for the same \p Metadata.
     */
    virtual QString getField(Metadata *m) = 0;	

    /** \fn Get the current depth during tree building.
        
        While \p MusicTreeBuilder::makeTree is recursing downwards to
        build the tree, this method will return the current depth and
        can/should be used in the subclass when implemented the
        virtual methods. 

        Ie. the directory builder can use this in isLeafDone to
        determine if the \p Metadata object under consideration has no
        more elements in it's path.
     */
    inline int getDepth(void) { return m_depth; }

  private:
    int m_depth;
};

#endif /* TREEBUILDERS_H_ */
