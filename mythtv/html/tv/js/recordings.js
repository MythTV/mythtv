
function recordingDeleted(chanID, startTime, unDeleted)
{
    var id = chanID + "_" + startTime + "_row";
    $(jq(id)).hide('slide',{direction:'left'},1000);
}

function deleteRecording(chanID, startTime, allowRerecord, forceDelete)
{
    hideMenu("optMenu");
    $("#wastebin").show();

    var reRecord = allowRerecord ? 1 : 0;
    var force = forceDelete ? 1 : 0;
    var url = "/tv/ajax_backends/dvr_util.qsp?action=deleteRecording&chanID=" + chanID + "&startTime=" + startTime +
              "&allowRerecord=" + reRecord + "&forceDelete=" + force;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingDeleted( response[0], response[1], false );
                            });
}

function unDeleteRecording(chanID, startTime)
{
    hideMenu("optMenu");

    var url = "/tv/ajax_backends/dvr_util.qsp?action=unDeleteRecording&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingDeleted( response[0], response[1], true );
                            });
}
