#include "mythcontext.h"

#include "imagemetadata.h"



/** \fn     ImageMetadata::ImageMetadata()
 *  \brief  Constructor
 *  \return void
 */
ImageMetadata::ImageMetadata()
{
    m_id = 0;
    m_fileName = "";
    m_name = "";
    m_path = "";
    m_parentId = 0;
    m_dirCount = 0;
    m_fileCount = 0;
    m_type = 0;
    m_modTime = 0;
    m_size = 0;
    m_extension = "";
    m_angle = 0;
    m_orientation = 0;
    m_date = 0;
    m_zoom = 100;
    m_isHidden = false;

    m_selected = false;

    m_thumbPath = "";
    m_thumbFileNameList = new QList<QString>();

    // Initialize the lists to avoid assertions.
    for (int i = 0; i < kMaxFolderThumbnails; ++i)
        m_thumbFileNameList->append(QString(""));
}



/** \fn     ImageMetadata::~ImageMetadata()
 *  \brief  Destructor
 *  \return void
 */
ImageMetadata::~ImageMetadata()
{
    if (m_thumbFileNameList)
        delete m_thumbFileNameList;
}



/** \fn     ImageMetadata::SetAngle(int)
 *  \brief  Sets the angle within the allowed range
 *  \param  angle The angle that shall be saved
 *  \return void
 */
void ImageMetadata::SetAngle(int angle)
{
    m_angle += angle;

    if (m_angle >= 360)
        m_angle -= 360;

    if (m_angle < 0)
        m_angle += 360;
}



/** \fn     ImageMetadata::SetZoom(int)
 *  \brief  Sets the zoom within the allowed range
 *  \param  zoom The zoom value that shall be saved
 *  \return void
 */
void ImageMetadata::SetZoom(int zoom)
{
    m_zoom += zoom;

    if (m_zoom > 300)
        m_zoom = 300;

    if (m_zoom < 0)
        m_zoom = 0;
}



/** \fn     ImageMetadata::GetOrientation()
 *  \brief  Gets the orientation of the image (rotated, vertically and/or
 *          horizontally flipped) depending on the old state.
 *  \return The new orientation
 */
int ImageMetadata::GetOrientation()
{
    return m_orientation;
}



/** \fn     ImageMetadata::SetOrientation(int)
 *  \brief  Sets the orientation of the image (rotated, vertically and/or
 *          horizontally flipped) depending on the old state.
 *  \param  orientation The orientation value that shall be set
 *  \return void
 */
void ImageMetadata::SetOrientation(int orientation, bool replace = false)
{
    if (replace)
    {
        m_orientation = orientation;
        return;
    }

    switch (m_orientation)
    {
    case 0: // The image has no orientation saved
    case 1: // If the image is in its original state
        if (orientation == kFileRotateCW)
            m_orientation = 8;
        else if (orientation == kFileRotateCCW)
            m_orientation = 6;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 2;
        else if (orientation == kFileFlipVertical)
            m_orientation = 4;
        break;

    case 2: // The image is horizontally flipped
        if (orientation == kFileRotateCW)
            m_orientation = 7;
        else if (orientation == kFileRotateCCW)
            m_orientation = 5;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 1;
        else if (orientation == kFileFlipVertical)
            m_orientation = 3;
        break;

    case 3: // The image is rotated 180°
        if (orientation == kFileRotateCW)
            m_orientation = 6;
        else if (orientation == kFileRotateCCW)
            m_orientation = 8;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 4;
        else if (orientation == kFileFlipVertical)
            m_orientation = 2;
        break;

    case 4: // The image is vertically flipped
        if (orientation == kFileRotateCW)
            m_orientation = 5;
        else if (orientation == kFileRotateCCW)
            m_orientation = 7;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 3;
        else if (orientation == kFileFlipVertical)
            m_orientation = 1;
        break;

    case 5: // The image is transposed (rotated 90° CW flipped horizontally)
        if (orientation == kFileRotateCW)
            m_orientation = 2;
        else if (orientation == kFileRotateCCW)
            m_orientation = 4;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 8;
        else if (orientation == kFileFlipVertical)
            m_orientation = 6;
        break;

    case 6: // The image is rotated 90° CCW
        if (orientation == kFileRotateCW)
            m_orientation = 1;
        else if (orientation == kFileRotateCCW)
            m_orientation = 3;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 7;
        else if (orientation == kFileFlipVertical)
            m_orientation = 5;
        break;

    case 7: // The image is transversed  (rotated 90° CW and flipped vertically)
        if (orientation == kFileRotateCW)
            m_orientation = 4;
        else if (orientation == kFileRotateCCW)
            m_orientation = 2;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 6;
        else if (orientation == kFileFlipVertical)
            m_orientation = 8;
        break;

    case 8: // The image is rotated 90° CW
        if (orientation == kFileRotateCW)
            m_orientation = 3;
        else if (orientation == kFileRotateCCW)
            m_orientation = 1;
        else if (orientation == kFileFlipHorizontal)
            m_orientation = 5;
        else if (orientation == kFileFlipVertical)
            m_orientation = 7;
        break;

    default:
        break;
    }
}
