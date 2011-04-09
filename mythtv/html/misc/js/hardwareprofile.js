function submitHardwareProfile() {
    var result  = 0;

    setStatusMessage("Submitting hardware profile...");

    $.post("/Myth/ProfileSubmit", {},
        function(data) {
            setStatusMessage("Hardware profile submitted successfully.");
            getHardwareProfileURL();
            getHardwareProfileUpdated();
            result = 1;
        }).error(function(data) {
            setErrorMessage("Unable to submit hardware profile!");
        });

    return result;
}

function deleteHardwareProfile() {
    var result  = 0;

    setStatusMessage("Deleting hardware profile...");

    $.post("/Myth/ProfileDelete", {},
        function(data) {
            setStatusMessage("Hardware profile deleted successfully.");
            getHardwareProfileURL();
            getHardwareProfileUpdated();
            result = 1;
        }).error(function(data) {
            setErrorMessage("Unable to delete hardware profile!");
        });

    return result;
}

function getHardwareProfileURL() {
    var result  = "";

    $.getJSON("/Myth/ProfileURL", {},
        function(data) {
            result = data.QString;
            var url = "<b>Profile Location:</b> " + result;
            $("#profileURL").html(url);
        });

    return result;
}

function getHardwareProfileUpdated() {
    var result  = "";

    $.getJSON("/Myth/ProfileUpdated", {},
        function(data) {
            result = data.QString;
            var update = "<b>Profile Updated:</b> " + result;
            $("#profileUpdated").html(update);
        });

    return result;
}

function getHardwareProfileText() {
    var result  = "";

    $.get("/Myth/ProfileText", {},
        function(data) {
            result = $(data).find("QString").text();
            result = result.replace(/\r?\n|\r/g, '<br>');
            $("#profile-text").html(result);
        });

    return result;
}

setupTabs("hardwareprofiletabs");
