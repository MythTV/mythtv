
function editJob(title, descKey, commandKey) {
    $('#editborder').attr({ class: 'editborder-fullscreen' }); 
    loadEditWindow("/setup/jobqueue-job-editor.html");

    $("#jobEditTitle").html(title);

    $("#jobEditDescKey").val(descKey);
    if ((title == "Transcoder") || (title == "Commercial Flagger"))
        $("#jobEditDesc").parent().html("<b>" + title + "</b>");
    else
        $("#jobEditDesc").val(getSetting("", descKey, ""));

    $("#jobEditCommandKey").val(commandKey);
    $("#jobEditCommand").val(getSetting("", commandKey, ""));
}

function appendMatch(match) {
    var newValue = $("#jobEditCommand").val() + match;
    $("#jobEditCommand").val(newValue);
}

function saveJobEditor() {
    var descKey;
    var commandKey;
    var value;
    var descSavedOK = 0;
    var cmdSavedOK = 0;
    var title = $("#jobEditTitle").html();

    if ((title != "Transcoder") && (title != "Commercial Flagger")) {
        descKey = $("#jobEditDescKey").val();
        value = $("#jobEditDesc").val();
        if (putSetting("", descKey, value))
            descSavedOK = 1;
    } else {
        descSavedOK = 1; /* We didn't really save, but pretend we did */
    }

    commandKey = $("#jobEditCommandKey").val();
    value = $("#jobEditCommand").val();
    if (putSetting("", commandKey, value))
        cmdSavedOK = 1;

    editJob($("#jobEditTitle").html(), descKey, commandKey);

    if (!descSavedOK)
        appendEditErrorMessage("Error updating job description");

    if (!cmdSavedOK)
        appendEditErrorMessage("Error updating job command");

    if (descSavedOK && cmdSavedOK)
        appendEditStatusMessage("Save Successful");
}

$("#setuptabs").tabs({ cache: true });

