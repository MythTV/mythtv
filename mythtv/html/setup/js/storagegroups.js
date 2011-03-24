var sgTabIDs = new Array();
var sgTabNames = new Array();
var sgTabCount = 0;

function appendTabRow(tabID, id, group, host, dir) {
    var rowNum = $('#sgtable-' + tabID + ' tr').length;
    var rowID = "sgtable-" + tabID + "-" + rowNum;
    $("#sgtable-" + tabID + " tr:last").after("<tr id='" + rowID + "'><td>" + id +"</td><td>" + host + "</td><td>" + dir + "</td><td><a href=\"javascript:removeStorageGroupTableRow(" + tabID + ", '" + rowID + "')\">Delete</a></tr>");
}

function initStorageGroups() {
    var selectedHost = $("#sgShowHost").val();

    $("#storagegrouptabs").tabs();

    while (sgTabCount > 0) {
        $("#storagegrouptabs").tabs("remove", 0);
        sgTabCount--;
    }
    sgTabIDs = new Array();
    sgTabNames = new Array();
    sgTabCount = 0;

    $.getJSON("/Myth/GetStorageGroupDirs", function(data) {
        var sgTabID;
        $.each(data.StorageGroupDirList.StorageGroupDirs, function(i, value) {
            if ((selectedHost != "ALL") &&
                (selectedHost != value.HostName)) {
                return true;
            }

            if (value.GroupName in sgTabIDs) {
                sgTabID = sgTabIDs[value.GroupName];
            } else{
                sgTabID = sgTabCount++;
                sgTabIDs[value.GroupName] = sgTabID;
                sgTabNames[sgTabID] = value.GroupName;
                $("#storagegrouptabs").tabs("add", "#sgtabs-" + sgTabID, value.GroupName, sgTabID);
                $("#sgtabs-" + sgTabID).html("<div id='sgtabs-" + sgTabID + "-add'><a href='javascript:addDir(" + sgTabID + ")'>Add Directory</a></div><div id='sgtabs-" + sgTabID + "-edit' style='display: none'><b>Adding new Storage Group Directory</b><br><table border=0 cellpadding=2 cellspacing=2><tr><th align=right>Host:</th><td>" + hostsSelect("sgtabs-" + sgTabID + "-edit-hostname") + "</td></tr><tr><th align=right>New Directory:</th><td><input id='sgtabs-" + sgTabID + "-edit-dirname' size=40><input type=button onClick='javascript:browseForNewDir(" + sgTabID + ")' value='Browse'><input type=hidden id='sgtabs-" + sgTabID + "-edit-groupname' value='" + value.GroupName + "'></td></tr><tr><td colspan=2><a href='javascript:saveDir(" + sgTabID + ")'>Save</a> <a href='javascript:cancelDir(" + sgTabID + ")'>Cancel</a></td></tr></table><hr></div><table id='sgtable-" + sgTabID + "' border=1 cellpadding=4 cellspacing=0 width='100%'><th>DirID</th><th>Host Name</th><th>Dir Name</th><th>Actions</th></tr></table><br><a href='javascript:deleteStorageGroup(" + sgTabID + ")'>Delete Storage Group</a>");
            }

            appendTabRow(sgTabID, value.Id, value.GroupNme, value.HostName, value.DirName);
        });
    });
}

function addDir(tabID) {
    $("#sgtabs-" + tabID + "-add").css("display", "none");
    $("#sgtabs-" + tabID + "-edit").css("display", "");
}

function saveDir(tabID) {
    var group = $("#sgtabs-" + tabID + "-edit-groupname").val();
    var host  = $("#sgtabs-" + tabID + "-edit-hostname").val();
    var dir   = $("#sgtabs-" + tabID + "-edit-dirname").val();

    if (addStorageGroupDir(group, dir, host)) {
        appendTabRow(tabID, 0, group, host, dir);
        $("#sgtabs-" + tabID + "-add").css("display", "");
        $("#sgtabs-" + tabID + "-edit").css("display", "none");
    }
}

function cancelDir(tabID) {
    $("#sgtabs-" + tabID + "-add").css("display", "");
    $("#sgtabs-" + tabID + "-edit").css("display", "none");
}

function deleteStorageGroup( tabID ) {
    alert("Are you sure?  Deleting Storage Groups is not hooked up yet anyway "
        + ", but you can delete all directories in a Storage Group "
        + "individually.");
}

function addStorageGroupDir( group, dir, host ) {
    var result = 0;

    // FIXME, validate input data here or in caller

    $.ajaxSetup({ async: false });
    $.post("/Myth/AddStorageGroupDir",
        { GroupName: group, DirName: dir, HostName: host},
        function(data) {
            if (data.SuccessFail.Result == "true")
                result = 1;
            else
                alert("data.SuccessFail.Result != true");
        }, "json").error(function(data) {
            alert("Error: unable to add Storage Group Directory");
        });
    $.ajaxSetup({ async: true });
    // FIXME, better alerting

    return result;
}

function removeStorageGroupDir( group, dir, host ) {
    var result = 0;

    // FIXME, validate input data here or in caller

    $.ajaxSetup({ async: false });
    $.post("/Myth/RemoveStorageGroupDir",
        { GroupName: group, DirName: dir, HostName: host},
        function(data) {
            if (data.SuccessFail.Result == "true")
                result = 1;
            else
                alert("data.SuccessFail.Result != true");
        }, "json").error(function(data) {
            alert("Error: unable to remove Storage Group Directory");
        });
    $.ajaxSetup({ async: true });
    // FIXME, better alerting

    return result;
}

function removeStorageGroupTableRow( tabID, rowID ) {
    var id = $("#" + rowID).find("td").eq(0).html();
    var group = sgTabNames[tabID];
    var host = $("#" + rowID).find("td").eq(1).html();
    var dir = $("#" + rowID).find("td").eq(2).html();

    if (removeStorageGroupDir(group, dir, host)) {
        $("#" + rowID).remove();
    } else {
        alert("json remove call failed.");
    }
}

function browseForNewDir( tabID ) {
    alert( "Browsing for new directories is not hooked up yet. "
        + "You will have to manually type in the directory name to add to "
        + "the " + sgTabNames[tabID] + " group.");
}

//////////////////////////////////////////////////////////////////////////////

initStorageGroups();

