var sgTabIDs = new Array();
var sgTabNames = new Array();
var sgTabCount = 0;

function appendTabRow(tabID, id, host, dir) {
    var rowNum = $('#sgtable-' + tabID + ' tr').length;
    var altText = "";
    if ((rowNum/2) % 1 == 0)
        altText = "class='alt' ";
    var rowID = "sgtable-" + tabID + "-" + rowNum;
    var afterWhat = "thead:last";
    if ($('#sgtable-' + tabID + ' tr').length > 0)
        afterWhat = "tr:last";
    $("#sgtable-" + tabID + " " + afterWhat).after("<tr " + altText + "id='" + rowID + "' class='ui-widget-content'><td class='invisible'>" + id +"</td><td>" + host + "</td><td>" + dir + "</td><td><input type='button' onClick=\"javascript:showConfirm('Are you sure you want to remove this group?', removeStorageGroupTableRow, ['" + tabID + "','" + rowID + "'])\" value='Delete'/></tr>");
}

function initStorageGroups(selectedGroup) {
    var selectedHost = $("#sgShowHost").val();

    var tabs = $("#storagegrouptabs").tabs();

    var ul = tabs.find("ul");
    $("#storagegrouptabs ul li").each(function() {
        var panelId = $(this).remove().attr("aria-controls");
        $("#" + panelId).remove();
    });

    sgTabIDs = new Array();
    sgTabNames = new Array();
    sgTabCount = 0;

    $.getJSON("/Myth/GetStorageGroupDirs", function(data) {
        var sgTabID;
        var sgHosts = {};
        $.each(data.StorageGroupDirList.StorageGroupDirs, function(i, value) {
            if (sgHosts[value.HostName.toLowerCase()] == undefined) {
                sgHosts[value.HostName.toLowerCase()] = 1;
            }

            if ((selectedHost != "ALL") &&
                (selectedHost.toLowerCase() != value.HostName.toLowerCase())) {
                return true;
            }

            if (value.GroupName in sgTabIDs) {
                sgTabID = sgTabIDs[value.GroupName];
            } else {
                sgTabID = sgTabCount++;
                sgTabIDs[value.GroupName] = sgTabID;
                sgTabNames[sgTabID] = value.GroupName;
                $("<li><a href='#sgtabs-" + sgTabID + "'>" + value.GroupName + "</a></li>").appendTo(ul);
                $("<div id='sgtabs-" + sgTabID + "'><input type=button value='Add Directory' onClick='javascript:addNewStorageGroupDir(" + sgTabID + ")'><table id='sgtable-" + sgTabID + "' width='100%'><thead class='ui-widget-header'><th class='invisible'>DirID</th><th>Host Name</th><th>Directory Path</th><th>Actions</th></thead></tr></table></div>").appendTo(tabs);
            }

            appendTabRow(sgTabID, value.Id, value.HostName, value.DirName);
        });

        if (selectedGroup)
            $("#storagegrouptabs").tabs("select", sgTabIDs[selectedGroup]);

        var sortedKeys = new Array();
        for (var i in sgHosts) {
            sortedKeys.push(i);
        }
        sortedKeys.sort();

        var sgHostSelect = "Host filter: <select id='sgShowHost' "
            + "onChange='javascript:initStorageGroups()'><option value='ALL'>"
            + "Display ALL Hosts</option>;";
        for (var i in sortedKeys) {
            sgHostSelect += "<option";

            if (selectedHost.toLowerCase() == sortedKeys[i])
                sgHostSelect += " selected";

            sgHostSelect += ">" + sortedKeys[i] + "</option>";
        }

        sgHostSelect += "</select>";
        $("#sgHostSelect").html(sgHostSelect);
        setUIAttributes();

        $("<li><a href='#sgtabs-addNewGroup'>+</a></li>").appendTo(ul);
        $("<div id='sgtabs-addNewGroup'></div>").appendTo(tabs);
        tabs.tabs("refresh");

        $("a[href='#sgtabs-addNewGroup']").click(function() {
            addNewStorageGroup();
        });
    });
}

function addNewStorageGroup() {
    loadEditWindow("/setup/storagegroups-add-dir.html");
    $("#sgGroupNameStatic").val("");
    $("#sgGroupNameCell").html("<input id='sgGroupName' size=30 />");
    $("#sgHostNameCell").html(hostsSelect("sgHostName"));
    $("#edit").dialog({
        modal: true,
        width: 670,
        height: 250,
        'title': 'Add New Storage Group',
        closeOnEscape: false,
        buttons: {
           'Save': function() { saveNewStorageGroup() },
           'Cancel': function() { $(this).dialog('close'); }
    }});
    $("#edit").dialog("open");
}

function addNewStorageGroupDir(tabID) {
    loadEditWindow("/setup/storagegroups-add-dir.html");
    $("#sgGroupNameStatic").val(sgTabNames[tabID]);
    $("#sgGroupNameCell").html(sgTabNames[tabID]);
    $("#sgHostNameCell").html(hostsSelect("sgHostName"));
    $("#edit").dialog({
        modal: true,
        width: 670,
        height: 250,
        'title': 'Add New Directory',
        closeOnEscape: false,
        buttons: {
           'Save': function() { saveNewStorageGroupDir(tabID) },
           'Cancel': function() { $(this).dialog('close'); }
    }});
    $("#edit").dialog("open");
}

function saveNewStorageGroupDir(tabID) {
    saveNewDir($("#sgGroupNameStatic").val(), tabID);
}

function saveNewStorageGroup() {
    saveNewDir($("#sgGroupName").val());
}

function saveNewDir(group, tabID) {
    var host   = $("#sgHostName").val();
    var dir    = $("#sgDirName").val();
    var errorMsg;

    if (group == "") {
        errorMsg = "Save failed. Storage Group name was not supplied.";
    } else if (host == "") {
        errorMsg = "Save failed. Host Name was not supplied.";
    } else if (dir == "") {
        errorMsg = "Save failed. Directory Name was not supplied.";
    }
    if (errorMsg != undefined) {
        setErrorMessage(errorMsg);
        return;
    }

    if (addStorageGroupDir(group, dir, host)) {
        $("#edit").dialog('close');
        if (tabID != undefined)
            appendTabRow(tabID, 0, host, dir);
        else
            initStorageGroups();
        setUIAttributes();
        setStatusMessage("Storage Group Directory save Succeeded.");
    } else {
        setErrorMessage("Storage Group Directory save Failed!");
    }
}

function removeStorageGroupTableRow( tabID, rowID ) {
    var id = $("#" + rowID).find("td").eq(0).html();
    var group = sgTabNames[tabID];
    var host = $("#" + rowID).find("td").eq(1).html();
    var dir = $("#" + rowID).find("td").eq(2).html();

    if (removeStorageGroupDir(group, dir, host)) {
        setStatusMessage("Remove Storage Group Directory Succeeded.");
        $("#" + rowID).remove();
        if ($('#sgtable-' + tabID + ' tr').length == 0) {
            initStorageGroups(); /* could optimize this and not reload */
        }
    } else {
        setErrorMessage("Remove Storage Group Directory Failed!");
    }
}

var sgNewDirInputID;
function newDirSelected(file) {
    $("#" + sgNewDirInputID).val(file);
}

function browseForNewDir( inputID ) {
    var dirs = new Array;
    sgNewDirInputID = inputID;
    openDirBrowser("Storage Directory", dirs, newDirSelected);
}

//////////////////////////////////////////////////////////////////////////////

setupNonAnimatedTabs("storagegrouptabs", false);
initStorageGroups();

