
function recordProgram(chanID, startTime, type)
{
    hideMenu("optMenu");
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=simpleRecord&chanID=" + chanID + "&startTime=" + startTime + "&type=" + type;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

// Override the one in common.js for now
function checkRecordingStatus(chanID, startTime)
{
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=checkRecStatus&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url ).done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                var id = response[0] + "_" + response[1];
                                var layer = document.getElementById(id);
                                if (!isValidObject(layer))
                                {
                                    setErrorMessage("Guide::checkRecordingStatus() invalid program ID (" + id + ")");
                                    return;
                                }
                                toggleClass(layer, "programScheduling");
                                // toggleClass(layer, response[2]);
                                var popup = document.getElementById(id + "_schedpopup");
                                toggleVisibility(popup);
                                // HACK: Easiest way to ensure that everything
                                //       on the page is correctly updated for now
                                //       is to reload
                                reloadGuideContent();
                            });
}

function reloadGuideContent()
{
    loadTVContent(currentContentURL, "guideGrid", "dissolve", {"GuideOnly": "1"});  // currentContentURL is defined in util.qjs
}

function pageLeft()
{
    changePage("left");
}

function pageRight()
{
    changePage("right");
}

function changePage(direction)
{
    var INTERVAL = 4; // 2 Hours, 30 Minute time periods
    var timeSelect = document.getElementById("guideStartTime");
    var timeIndex = timeSelect.selectedIndex;
    // oldIndex lets us adjust the animation depending on whether we're moving
    // backwards or forwards in time
    timeSelect.setAttribute('data-oldIndex', timeIndex);
    var dateSelect = document.getElementById("guideStartDate");
    var dateIndex = dateSelect.selectedIndex;
    // oldIndex lets us adjust the animation depending on whether we're moving
    // backwards or forwards in time
    dateSelect.setAttribute('data-oldIndex', dateIndex);

    if (direction == "left")
    {
        if ((timeIndex - INTERVAL) < 0)
        {
            timeIndex = (timeIndex - INTERVAL) + timeSelect.length;
            dateIndex = dateIndex - 1;

            if (dateIndex < 0)
            {
                timeIndex = 0;
                dateIndex = 0;
            }
        }
        else
            timeIndex = (timeIndex - INTERVAL);
    }
    else if (direction == "right")
    {
        if ((timeIndex + INTERVAL) >= timeSelect.length)
        {
            timeIndex = (timeIndex + INTERVAL) - timeSelect.length;
            dateIndex = dateIndex + 1;

            if (dateIndex >= dateSelect.length)
            {
                timeIndex = (timeSelect.length - 1);
                dateIndex = (dateSelect.length - 1);
            }
        }
        else
            timeIndex = timeIndex + INTERVAL;
    }

    dateSelect.selectedIndex = dateIndex;
    timeSelect.selectedIndex = timeIndex;
    // If the date has changed, then use the dateSelect onChange handler
    // instead
    if (dateSelect.getAttribute('data-oldIndex') != dateSelect.selectedIndex)
        dateSelect.onchange();
    else
        timeSelect.onchange();
}

function changeGuideStartTime(selectBox)
{
    var oldIndex = selectBox.getAttribute('data-oldIndex');

    if (typeof oldIndex === "undefined" || oldIndex == null)
        oldIndex = selectBox.defaultIndex;

    // oldIndex lets us adjust the animation depending on whether we're moving
    // backwards or forwards in time
    selectBox.setAttribute('data-oldIndex', selectBox.selectedIndex);

    var transition = (selectBox.selectedIndex > oldIndex) ? 'left' : 'right';
    console.log("New " + selectBox.selectedIndex + " Old " + oldIndex + " Transition " + transition);

    submitForm(selectBox.form, 'guideGrid', transition);//     fixGuideHeight();
}

function submitGuideForm(context)
{
    submitForm(context.form, 'guideGrid', 'dissolve');
}
