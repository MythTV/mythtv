
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
    var helpHeight = $("#helpWindow").parent().height();
    $("#helpWindow").dialog("option", "position",
        [(window.innerWidth - 330), (window.innerHeight - (helpHeight + 30))]);
    $("#helpWindow").show();
    $("#helpWindow").draggable();
}

function hideHelpWindow() {
    $("#helpWindow").hide();
}

function showHelp(title, content) {
    $("#helpWindow").dialog({
      'title': title,
      'width': 300,
      'height': 'auto',
      'minHeight': 20,
      'position': [(window.innerWidth - 330), window.innerHeight]
    });
    $("#helpWindow").html(content);
    showHelpWindow();
}

function showSettingHelp(setting) {
    var title = settingsInfo[setting].attr("label");
    var content = settingsInfo[setting].attr("help_text");
    showHelp(title, content);
}

function showEditWindow() {
    $("#edit").show();
}

function hideEditWindow() {
    $("#edit").hide();
}

function submitConfigForm(form) {
    var data = $("#config_form_" + form).serialize();
    var url = $("#__config_form_action__").val();
    var savedOK = 1;

    /* Clear error messages */
    $("#config_form_" + form + " :input").each(function() {
        $("#" + $(this).attr("id") + "_error").html("");
    });

    $.ajaxSetup({ async: false });
    $.post(url, data, function(data) {
        $.each(data, function(key, value) {
            $("#" + key + "_error").html(value);

            if (value != "")
                savedOK = 0;
        });
    }, "json");
    $.ajaxSetup({ async: true });

    if (savedOK)
        setStatusMessage("Changes saved successfully");
    else
        setErrorMessage("Error saving changes!");
}

function setSettingInputValues(divName) {
    $("#" + divName + " :input").each(function() {
        if (($(this).attr("type") != "button") &&
            ($(this).attr("type") != "submit") &&
            ($(this).attr("type") != "reset") &&
            (settingsList[$(this).attr("id")])) {
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

function storeSetting(setting) {
    settingsInfo[setting.attr("value")] = setting;
}

function storeGroupSettings(group) {
    group.find("group").each(function() { storeGroupSettings($(this)); });
    group.find("setting").each(function() { storeSetting($(this)); });
}

function loadConfigXML() {
  settingsInfo = {};
  $.get("/Config/Database/XML", function(xml) {
        $(xml).find("group").each(function() { storeGroupSettings($(this)); });
        $(xml).find("setting").each(function() { storeSetting($(this)); });
    }).error(function() { alert("Error downloading /Config/Database XML");});

  $.get("/Config/General/XML", function(xml) {
        $(xml).find("group").each(function() { storeGroupSettings($(this)); });
        $(xml).find("setting").each(function() { storeSetting($(this)); });
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
getSettingsList();  /* load now to fix a race condition */
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
            $.each(data.StringList, function(i, value) {
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

/****************************************************************************/
/* Storage Groups access methods                                            */
/****************************************************************************/
function addStorageGroupDir( group, dir, host ) {
    var result = 0;

    // FIXME, validate input data here or in caller

    $.ajaxSetup({ async: false });
    $.post("/Myth/AddStorageGroupDir",
        { GroupName: group, DirName: dir, HostName: host},
        function(data) {
            if (data.bool == "true")
                result = 1;
        }, "json");
    $.ajaxSetup({ async: true });

    return result;
}

function removeStorageGroupDir( group, dir, host ) {
    var result = 0;

    // FIXME, validate input data here or in caller

    $.ajaxSetup({ async: false });
    $.post("/Myth/RemoveStorageGroupDir",
        { GroupName: group, DirName: dir, HostName: host},
        function(data) {
            if (data.bool == "true")
                result = 1;
        }, "json");
    $.ajaxSetup({ async: true });

    return result;
}

/****************************************************************************/
/* File/Directory Browser                                                   */
/****************************************************************************/
var fileBrowserCallback;

function fileBrowserEntryInfoCallback(file) {
    /* do nothing here for now */
}

function openFileBrowserWindow(title, dirs, saveCallback, onlyDirs, storageGroup) {
    var fileBrowserWidth = 330;
    var rootPath = "/";
    if (storageGroup != undefined && storageGroup != "") {
        rootPath = "myth://" + storageGroup + "@" + getHostName() + ":"
                 + location.port + "/";
    }

    $("#fileBrowserWindow").dialog({
        modal: true,
        width: fileBrowserWidth,
        height: 535,
        'title': title,
        closeOnEscape: false,
        buttons: {
           'Save': saveFileBrowser,
           'Cancel': function() { $(this).dialog('close'); }
        }
    });

    $('#fileBrowserWindow').dialog("open");
    $.ajaxSetup({ async: false });
    if (typeof jQuery.fileTree == 'undefined') {
        $.getScript("/3rdParty/jquery/jqueryFileTree/jqueryFileTree.js");
    }
    $.ajaxSetup({ async: true });
    fileBrowserCallback = saveCallback;
        
    $('#fileBrowser').fileTree(
      { root: rootPath,
        script: '/Config/FileBrowser',
        dirsOnly: onlyDirs
      }, fileBrowserEntryInfoCallback);
}

function openFileBrowser(title, dirs, callback) {
    openFileBrowserWindow(title, dirs, callback, 0);
}

function openDirBrowser(title, dirs, callback) {
    openFileBrowserWindow(title, dirs, callback, 1);
}

function openStorageGroupBrowser(title, callback, group) {
    var dirs = new Array();
    openFileBrowserWindow(title, dirs, callback, 0, group);
}

function saveFileBrowser() {
    var selectedItem = $('#fileBrowser').find('A.selected').attr("rel");
    if (selectedItem && fileBrowserCallback)
    {
        $('#fileBrowserWindow').dialog('close');
        if (typeof fileBrowserCallback == "string")
            $("#" + fileBrowserCallback).val(selectedItem);
        else
            fileBrowserCallback(selectedItem);
    }
    else
        alert("No item selected.");
}

