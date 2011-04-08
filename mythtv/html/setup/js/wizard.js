function testDBSettings() {
    var result = 0;
    var host = $("#dbHostName").val();
    var user = $("#dbUserName").val();
    var pass = $("#dbPassword").val();
    var name = $("#dbName").val();
    var port = $("#dbPort").val();

    clearEditMessages();

    if (name == null)
        name = "mythconverg";

    if (port == null)
        port = 3306;

    $.post("/Myth/TestDBSettings",
        { HostName: host, UserName: user, Password: pass, DBName: name, dbPort: port},
        function(data) {
            if (data.bool == "true") {
                result = 1;
                setEditStatusMessage("Database connection succeeded!");
            }
            else
                setEditErrorMessage("Database connection failed!");
        }, "json").error(function(data) {
            setEditErrorMessage("Database connection failed!");
        });

    return result;
}

function validateSettingsInDiv(divName) {
    var result = true;
    $("#" + divName + " :input").each(function() {
        if (($(this).attr("type") != "button") &&
            ($(this).attr("type") != "submit") &&
            ($(this).attr("type") != "reset")) {
            if (!validateSetting($(this).attr("id")))
                result = false;
        }
    });

    return result;
}

function saveWizard() {
    if (!validateSettingsInDiv("wizard-network")) {
        setEditErrorMessage("Network Setup has an error.");
    }

    alert("Saving is not fully functional, the database has not been modified!");
}

setupTabs("wizardtabs");
$("#edit").dialog({
    modal: true,
    width: 850,
    height: 500,
    'title': 'Setup Wizard',
    closeOnEscape: false,
    buttons: {
       'Save': function() {},
       'Cancel': function() { $(this).dialog('close'); }
    }
});
showEditWindow();

