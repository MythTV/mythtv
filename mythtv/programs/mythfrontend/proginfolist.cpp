
 // MythTV
#include "libmythbase/mythdate.h"

// MythFrontend
#include "proginfolist.h"

/*!
 \brief Initialise buttonlist from XML
 \param focusable Set if info list should be focusable (for scrolling)
 \return bool True initialization succeeds
*/
bool ProgInfoList::Create(bool focusable)
{
    bool err = false;
    UIUtilE::Assign(&m_screen, m_btnList, "infolist", &err);
    if (err)
        return false;

    m_btnList->SetVisible(true);

    m_btnList->SetCanTakeFocus(focusable);
    return true;
}

/*!
 \brief Toggle infolist state. Focusable widgets toggle between
 Basic & Full info. Non-focusable widgets toggle between Basic & Off.
*/
void ProgInfoList::Toggle(void)
{
    // Only focusable lists have an extra 'full' state as they can
    // be scrolled to view it all
    if (m_btnList->CanTakeFocus())
    {
        // Start showing basic info then toggle between basic/full
        m_infoVisible = (m_infoVisible == kLevel1 ? kLevel2 : kLevel1);
    }
    else if (m_infoVisible == kLevel1) // Toggle between off/basic
    {
        m_infoVisible = kNone;
        m_btnList->SetVisible(false);
        return;
    }
    else
    {
        m_infoVisible = kLevel1;
    }

    Clear();

    m_btnList->SetVisible(true);
}


/*!
 \brief Remove infolist from display
 \return bool True if buttonlist was displayed/removed
*/
bool ProgInfoList::Hide()
{
    // Only handle event if info currently displayed
    bool handled = (m_btnList && m_infoVisible != kNone);
    m_infoVisible = kNone;

    if (m_btnList)
        m_btnList->SetVisible(false);

    return handled;
}


/*!
 \brief Populate a buttonlist item with name & value
 \param name
 \param value
*/
void ProgInfoList::CreateButton(const QString& name, const QString& value)
{
    if (value.isEmpty())
        return;

    auto *item = new MythUIButtonListItem(m_btnList, "");

    InfoMap infoMap;
    infoMap.insert("name", name);
    infoMap.insert("value", value);

    item->SetTextFromMap(infoMap);
}


/*!
 * \brief Build list of key:value buttons
 * \param data key/value list
 */
void ProgInfoList::Display(const DataList& data)
{
    Clear();

    // Create buttons for each data pair
    for (const auto & pi : std::as_const(data))
    {
        if (m_infoVisible != kNone && std::get<2>(pi) <= m_infoVisible)
            CreateButton(std::get<0>(pi), std::get<1>(pi));
    }

    // Only give list focus if requested
    if (m_btnList->CanTakeFocus())
        m_screen.SetFocusWidget(m_btnList);
}
