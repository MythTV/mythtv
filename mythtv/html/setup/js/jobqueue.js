
function editJob(title, descKey, commandKey) {
    loadEditWindow("/setup/jobqueue-job-editor.html");

    $("#edit").dialog({
      'title': title,
      modal:   true,
      width:   850,
      height:  500,
      buttons: {
         'Save': function() {
                                saveJobEditor(); 
                                $(this).dialog('close');
                            },
         'Cancel': function() { $(this).dialog('close'); }
      }
    });
 
    $("#jobEditDescKey").val(descKey);
    if ((title == "Transcoder") || (title == "Commercial Flagger"))
        $("#jobEditDesc").parent().html("<b>" + title + "</b>");
    else
        $("#jobEditDesc").val(getSetting("", descKey, ""));

    $("#jobEditCommandKey").val(commandKey);
    $("#jobEditCommand").val(getSetting("", commandKey, ""));
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

    var errorMessage = "";
    if (!descSavedOK)
        errorMessage += "Error updating job description";

    if (!cmdSavedOK)
    {
        if (errorMessage.length)
            errorMessage += "<br>";
        errorMessage += "Error updating job command";
    }

    if (errorMessage.length)
        setErrorMessage(errorMessage);

    if (descSavedOK && cmdSavedOK)
        setStatusMessage("Save Successful");
}

setupTabs("jobqueuetabs");

