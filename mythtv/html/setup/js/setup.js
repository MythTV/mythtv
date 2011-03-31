
function basename(path) {
    return path.replace( /\\/g, '/').replace( /.*\//, '' );
}

function dirname(path) {
    return path.replace( /\\/g, '/').replace( /\/[^\/]*$/, '' );;
}

function parentDirName(path) {
    return basename(dirname(path));
}

function setupPageName(path) {
    return basename(path).replace( /\\/g, '/').replace( /\.[^\.]$/, '' );
}

function showHelpWindow() {
    $("#helpWindow").show();
    $("#helpWindow").draggable();
}

function hideHelpWindow() {
    $("#helpWindow").hide();
}

function showHelp(title, content) {
    $("#helpTitle").html(title);
    $("#helpContent").html("FIXME: you can drag this window....  " + content);
    showHelpWindow();
}

function showEditWindow() {
    $("#edit-bg").show();
    $("#editborder").show();
}

function hideEditWindow() {
    $("#editborder").hide();
    $("#edit-bg").hide();
    $("#editsavebutton").hide();
}

function clearEditMessages() {
    setEditStatusMessage("");
    setEditErrorMessage("");
}

function setEditStatusMessage(message) {
    $("#editErrorMessage").html("");
    $("#editStatusMessage").html(message);
    setTimeout('$("#editStatusMessage").html("")', statusMessageTimeout);
}

function setEditErrorMessage(message) {
    $("#editStatusMessage").html("");
    $("#editErrorMessage").html(message);
    setTimeout('$("#editErrorMessage").html("")', errorMessageTimeout);
}

function submitConfigForm(form) {
    var data = $("#config_form_" + form).serialize();
    var url = $("#__config_form_action__").val();

    /* FIXME, clear out _error divs */
    $.ajaxSetup({ async: false });
    $.post(url, data, function(data) {
        $.each(data, function(key, value) {
            $("#" + key + "_error").html(value);
        });
    }, "json");
    $.ajaxSetup({ async: true });
}

function setSettingInputValues(divName) {
    $("#" + divName + " :input").each(function() {
        if (($(this).attr("type") == "text") ||
            ($(this).attr("type") == "hidden")) {
            $(this).val(settingsList[$(this).attr("id")]);
        }
    });
}

function makeLocalBackendTheMaster() {
    $("#MasterServerIP").val(settingsList["BackendServerIP"]);
    $("#MasterServerIP_cell").html("<b>" + settingsList["BackendServerIP"]
        + "</b>");
}

/****************************************************************************/
/* /Config/* support routines and vars                                      */
/****************************************************************************/
var settingsList = {};
var settingsInfo = {};
var settingsDatabaseXML;
var settingsGeneralXML;

function storeSetting(setting) {
    settingsInfo[setting.attr("value")] = setting;
}

function storeGroupSettings(group) {
    group.find("group").each(function() { storeGroupSettings($(this)); });
    group.find("setting").each(function() { storeSetting($(this)); });
}

function loadConfigXML() {
  $.ajax({
    type: "GET",
    url: "/Config/Database/XML",
    dataType: "xml",
    success: function(xml) {
        settingsDatabaseXML = xml;
        $(xml).find("group").each(function() { storeGroupSettings($(this)); });
        $(xml).find("setting").each(function() { storeSetting($(this)); });
    }
  }).error(function() { alert("Error downloading /Config/Database XML");});

  $.ajax({
    type: "GET",
    url: "/Config/General/XML",
    dataType: "xml",
    success: function(xml) {
        settingsGeneralXML = xml;
        $(xml).find("group").each(function() { storeGroupSettings($(this)); });
        $(xml).find("setting").each(function() { storeSetting($(this)); });
    }
  }).error(function() { alert("Error downloading /Config/General XML");});
}

function getSettingsList() {
    $.ajaxSetup({ async: false });
    $.getJSON("/Config/Database/Settings", function(data) {
        $.each(data, function(key, value) {
            settingsList[key] = value;
        });
    });

    $.getJSON("/Config/General/Settings", function(data) {
        $.each(data, function(key, value) {
            settingsList[key] = value;
        });
    });
    $.ajaxSetup({ async: true });
}

function setInputErrorMessage(key, message) {
    $("#" + key + "_error").html(message);
}

function validateSetting(key) {
    if (settingsInfo[key] == undefined)
        return false;

    var item = settingsInfo[key];
    var type = settingsInfo[key].attr("data_type");

    if (type == "integer_range")
    {
        var range_min = parseInt(item.attr("range_min"));
        var range_max = parseInt(item.attr("range_max"));
        var newValue = parseInt($("#" + key).val());

        setInputErrorMessage(key, "");
        if ((newValue < range_min) || (newValue > range_max))
        {
            setInputErrorMessage(key, newValue + " is out of range.");
            return false;
        }
    }

    return true;
}

/* load the settings list when we load setup.js */
setTimeout(getSettingsList, 10);
/* load the XML we use to validate the new setting values */
setTimeout(loadConfigXML, 10);

/****************************************************************************/
function getOptionList(url, selected) {
    var options = "";
    $.ajaxSetup({ async: false });
    $.getJSON(url, function(data) {
        $.each(data, function(k1, v1) {
            $.each(v1, function(k2, v2) {
                options += "<option value='" + v2 + "'";
                if (v2 == selected)
                    options += " selected";
                options += ">" + v2 + "</option>";
            });
        });
    });
    $.ajaxSetup({ async: true });

    return options;
}

var hostOptions = "";
function getHostsOptionList() {
    if (hostOptions.length == 0) {
        hostOptions = "";
        $.ajaxSetup({ async: false });
        $.getJSON("/Myth/GetHosts", function(data) {
            $.each(data.QStringList, function(i, value) {
                hostOptions += "<option value='" + value + "'>" + value + "</option>";
            });
        });
        $.ajaxSetup({ async: true });
    }

    return hostOptions;
}

function hostsSelect(id) {
    var result = "<select id='" + id + "'>";
    result += getHostsOptionList();
    result += "</select>";

    return result;
}

