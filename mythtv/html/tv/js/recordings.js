
function recordingDeleted(recordedId, unDeleted)
{
    var id = recordedId + "_row";
    console.log("Recording Deleted: " + id);
    $(jq(id)).hide('slide',{direction:'left'},1000);
}

function deleteRecording(recordedId, allowRerecord, forceDelete)
{
    hideMenu("optMenu");
    $("#wastebin").show();

    var reRecord = allowRerecord ? 1 : 0;
    var force = forceDelete ? 1 : 0;
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=deleteRecording&RecordedId=" + recordedId +
              "&allowRerecord=" + reRecord + "&forceDelete=" + force;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                recordingDeleted( ajaxRequest.responseText, false );
                            });
}

function unDeleteRecording(recordedId)
{
    hideMenu("optMenu");

    var url = "/tv/ajax_backends/dvr_util.qsp?_action=unDeleteRecording&RecordedId=" + recordedId;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                recordingDeleted( ajaxRequest.responseText, true );
                            });
}

function playInBrowser(recordedId)
{
    loadContent("/tv/tvplayer.qsp?RecordedId=" + recordedId);
}

function playOnFrontend(recordedId, ip)
{
    var surl = "http://" + ip + ":6547/Frontend/PlayRecording?RecordedId=" + recordedId;
    $.ajax({ url: surl, type: 'POST' });
}
