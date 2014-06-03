
function recordingDeleted(chanID, startTime, unDeleted)
{
    var id = chanID + "_" + startTime + "_row";
    $(jq(id)).hide('slide',{direction:'left'},1000);
}

function deleteRecording(chanID, recStartTime, allowRerecord, forceDelete)
{
    hideMenu("optMenu");
    $("#wastebin").show();

    var reRecord = allowRerecord ? 1 : 0;
    var force = forceDelete ? 1 : 0;
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=deleteRecording&chanID=" + chanID + "&startTime=" + recStartTime +
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

    var url = "/tv/ajax_backends/dvr_util.qsp?_action=unDeleteRecording&chanID=" + chanID + "&startTime=" + recStartTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingDeleted( response[0], response[1], true );
                            });
}

function playInBrowser(chanID, recStartTime)
{
    loadContent("/tv/tvplayer.qsp?ChanId=" + chanID + "&StartTime=" + recStartTime);
}

function playOnFrontend(chanID, recStartTime, ip)
{
    var surl = "http://" + ip + ":6547/Frontend/PlayRecording?ChanID=" + chanID +
            "&StartTime=" + recStartTime;
    $.ajax({ url: surl, type: 'POST' });
}
