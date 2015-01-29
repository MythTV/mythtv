
/*!
 * \namespace MythChannelEditor
 * \brief Namespace for functions used in the 'General' page, guide.qsp
 */
var MythChannelEditor = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
        console.log("MythChannelEditor Init");
        InitChannelEditor();
        InitSourceList();
    };

    /*!
    * \fn Destructor
    * \public
    */
    this.Destructor = function()
    {
        // Stop timers, unregister event listeners etc here
    };

   /*!
    * \fn ReloadChannelGrid
    * \public
    */
    this.ReloadChannelGrid = function ()
    {
        var sourceid = $("#sourceList").val();

        $('#channels').setGridParam({
            url:'/Channel/GetChannelInfoList?Details=1&SourceID=' + sourceid,
            datatype:'json',
            page:1});
        $('#channels').trigger("reloadGrid");
    }

   /*!
    * \fn EditChannel
    * \public
    */
    this.EditChannel = function ()
    {
        var rowNum = $('#channels').getGridParam('selrow');
        var rowArr = $('#channels').getGridParam('selarrrow');
        if (rowNum != null) {
            if (rowArr.length == 1) {
                EditSelectedChannel();
            }
            else {
                EditMultiChannel();
            }
        }
    }

   /*!
    * \fn SaveChannelEdits
    * \public
    */
    this.SaveChannelEdits = function ()
    {
        var mplexid = $("#mplexList").val();
        var sourceid = $("#channelDetailSettingSourceId").html();
        var chanid = $("#channelDetailSettingChanId").html()

        var callsign = $.trim($("#channelDetailSettingCallSign").val());
        var channelname = $.trim($("#channelDetailSettingChannelName").val());
        var channum =  $.trim($("#channelDetailSettingChanNum").val());
        var serviceid =  $("#channelDetailSettingServiceId").val();
        var atscmajorchannel =  $("#channelDetailSettingATSCMajorChannel").val();
        var atscminorchannel =  $("#channelDetailSettingATSCMinorChannel").val();

        var useeit = false;

        if ($("#channelDetailSettingUseEIT").attr("checked")) {
            useeit = true;
        }

        var visible = false;

        if ($("#channelDetailSettingVisible").attr("checked")) {
            visible = true;
        }

        var frequencyid = $("#channelDetailSettingFrequencyId").val();
        var icon = $("#channelDetailSettingIconURL").val();
        var format = $("#channelDetailSettingFormat").val();
        var xmltvid = $("#channelDetailSettingXMLTVID").val();
        var defaultauth = $("#channelDetailSettingDefaultAuthority").val();

        if ($("#channels").jqGrid('setRowData', chanid,
            { MplexId: mplexid, SourceId: sourceid, ChanId: chanid,
            CallSign: callsign, ChannelName: channelname, ChanNum: channum,
            ServiceId: serviceid, ATSCMajorChan: atscmajorchannel,
            ATSCMinorChan: atscminorchannel, UseEIT: useeit, Visible: visible,
            FrequencyId: frequencyid, IconURL: icon, Format: format, XMLTVID: xmltvid,
            DefaultAuth: defaultauth }))
            {
                $.post("/Channel/UpdateDBChannel",
                    { MplexID: mplexid, SourceID: sourceid, ChannelID: chanid,
                    CallSign: callsign, ChannelName: channelname, ChannelNumber: channum,
                    ServiceID: serviceid, ATSCMajorChannel: atscmajorchannel,
                    ATSCMinorChannel: atscminorchannel, UseEIT: useeit, visible: visible,
                    FrequencyID: frequencyid, Icon: icon, Format: format, XMLTVID: xmltvid,
                    DefaultAuthority: defaultauth},
                    function(data) {
                        if (data.bool == "true") {
                            setStatusMessage("Channel updated successfully!");
                            $("#edit").dialog('close');
                        }
                        else
                            setErrorMessage("Channel update failed!");
                    }, "json");

                setStatusMessage("Updating channel...");
            }
    }

   /*!
    * \fn SaveMultiChannelEdits
    * \public
    */
    this.SaveMultiChannelEdits = function ()
    {
        var action = $("#multiChannelOptions").val();
        var rowArr = $('#channels').getGridParam('selarrrow');
        $.each(rowArr, function(i, value) {
            var rowdata = $("#channels").jqGrid('getRowData', value);
            var orig = '';
            var news = '';
            if (action == "ChanNum") {
                orig = rowdata.ChanNum;
                rowdata.ChanNum = MultiChannelEditTextReplace(rowdata);
                news = rowdata.ChanNum;
            }
            else if (action == "ChanName") {
                orig = rowdata.ChannelName;
                rowdata.ChannelName = MultiChannelEditTextReplace(rowdata);
                news = rowdata.ChannelName;
            }
            else if (action == "CallSign") {
                orig = rowdata.CallSign;
                rowdata.CallSign = MultiChannelEditTextReplace(rowdata);
                news = rowdata.CallSign;
            }

            if (UpdateChannelRow(value, rowdata)) {
            }
        });
        $("#edit").dialog('close');
    }

   /*!
    * \fn BrowseForNewChanIcon
    * \public
    */
    this.BrowseForNewChanIcon = function (url)
    {
        openStorageGroupBrowser("Channel Icon", ChooseNewChanIcon,
                                "ChannelIcons");
    }

   /*!
    * \fn PromptToDeleteChannel
    * \public
    */
    this.PromptToDeleteChannel = function ()
    {
        var message = "Are you sure you want to delete these channels?  This cannot be undone.";
        var rowNum = $('#channels').getGridParam('selrow');
        var rowArr = $('#channels').getGridParam('selarrrow');
        if (rowNum != null) {
            if (rowArr.length == 1) {
                message = "Are you sure you want to delete this channel?  This cannot be undone.";
            }
            showConfirm(message, MythChannelEditor.DeleteSelectedChannel);
        }
    }

   /*!
    * \fn DeleteSelectedChannel
    * \public
    */
    this.DeleteSelectedChannel = function ()
    {
        var rowArray = $('#channels').jqGrid('getGridParam','selarrrow');
        if (rowArray.length > 0) {
            var len = rowArray.length;
            for (var i = len - 1; i >= 0; i--) {
                var chanid = rowArray[i];
                if ($("#channels").jqGrid('delRowData', chanid)) {
                    $.post("/Channel/RemoveDBChannel",
                    { ChannelID: chanid },
                    function(data) {
                        if (data.bool == "true") {
                            setStatusMessage("Channel deleted successfully!");
                        }
                        else
                            setErrorMessage("Channel delete failed!");
                    }, "json");
                }
            }
            $('#channels').trigger('reloadGrid');
        }
    }

   /*!
    * \fn SearchChannelIcon
    * \public
    */
    this.SearchChannelIcon = function ()
    {
        var iconsSeen = [];
        iconCount = 0;

        var iconBrowser = $("#iconBrowser");
        iconBrowser.dialog({
            modal: true,
            width: 680,
            height: 340,
            'title': 'Channel Icon Download',
            closeOnEscape: false,
            autoOpen: false,
            buttons: {
            'Cancel': function() { $(this).dialog('close'); }
        }});

        iconBrowser.html("");
        $.getJSON("https://services.mythtv.org/channel-icon/lookup?jsoncallback=?",
            { callsign: $("#channelDetailSettingCallSign").val() },
            function(data) {
                $.each(data.urls, function(i, url) {
                    MythChannelEditor.AddChannelIconToGrid(url);
                });
            });

        $.getJSON("https://services.mythtv.org/channel-icon/lookup?jsoncallback=?",
            { xmltvid: $("#channelDetailSettingXMLTVID").val() },
            function(data) {
                $.each(data.urls, function(i, url) {
                    MythChannelEditor.AddChannelIconToGrid(url);
                });
            });

        iconBrowser.dialog("open");
    }

   /*!
    * \fn SelectChannelIcon
    * \public
    */
    this.SelectChannelIcon = function (url)
    {
        $("#channelDetailSettingIconPreview").attr('src', '/images/blank.gif');
        DownloadChannelIcon(url);
        $("#channelDetailSettingIconURL").val(basename(url));
        $("#iconBrowser").dialog("close");
    }

   /*!
    * \fn AddChannelIconToGrid
    * \public
    */
    this.AddChannelIconToGrid = function (url)
    {
        var result = "";
        if (url.substring(0,5).toLowerCase() == "error")
            return;

        this.iconRow = "row_" + parseInt(iconCount / 3);

        if (this.iconCount == 0) {
            $("#iconBrowser").html(
                "<table id='iconGallery' border=0 cellpadding=0 cellspacing=1>" +
                "<tr><td class='xinvisible'>" +
                        "<img src='/images/blank.gif' width=200 height=0></td>" +
                    "<td class='xinvisible'>" +
                        "<img src='/images/blank.gif' width=200 height=0></td>" +
                    "<td class='xinvisible'>" +
                        "<img src='/images/blank.gif' width=200 height=0></td></tr>" +
                "<tr id='" + iconRow + "'>" + ChannelIconLink(url, this.iconCount) +
                "</tr></table>");
        } else if (((this.iconCount % 3) == 0)) {
            $("#iconGallery").append("<tr id='" + this.iconRow + "'>" +
                ChannelIconLink(url, this.iconCount) + "</tr>");
        } else {
            $("#" + this.iconRow).append(ChannelIconLink(url, this.iconCount));
        }

        this.iconCount++;
    }

   /*!
    * \fn ResizeChannelIcon
    * \public
    */
    this.ResizeChannelIcon = function (id, maxWidth, maxHeight)
    {
        var preview = $("#channelDetailSettingIconPreview");
        if (preview.attr('src') != '/images/blank.gif') {
            preview.css('display', 'inline');
        }
        else {
            preview.css('display', 'none');
        }
        var ratio = 0;
        var icon = $("#" + id);
        var width = icon.width();
        var height = icon.height();

        if (maxWidth == undefined)
            maxWidth = 200;
        if (maxHeight == undefined)
            maxHeight = 200;

        $("#" + id + "_size").html("Size: " + width + "x" + height);

        if (width > maxWidth) {
            ratio = maxWidth / width;
            icon.attr("width", maxWidth);
            icon.attr("height", height * ratio);
            height = height * ratio;
            width = width * ratio;
        }

        if (height > maxHeight) {
            ratio = maxHeight / height;
            icon.attr("height", maxHeight);
            icon.attr("width", width * ratio);
            width = width * ratio;
        }
    }

///////////////////////////////////////////
// Private Functions
///////////////////////////////////////////

    /*!
    * \fn InitChannelEditor
    * \private
    */
    var InitChannelEditor = function ()
    {
        var sourceid = $("#sourceList").val();

        $("#channels").jqGrid({
            url:'/Channel/GetChannelInfoList?Details=1&SourceID=' + sourceid,
            datatype: 'json',
            colNames:['', 'Channel ID', 'Channel Number', 'Callsign', 'Channel Name', 'XMLTVID', 'Visible', 'Icon Path',
                    'Multiplex ID', 'Transport ID', 'Service ID', 'Network ID', 'ATSC Major Channel',
                    'ATSC Minor Channel', 'Format', 'Modulation', 'Frequency', 'Frequency ID', 'Frequency Table',
                    'Fine Tuning', 'SI Standard', 'Channel Filters', 'Source ID', 'Input ID', 'Commercial Free',
                    'Use On Air Guide', 'Default Authority'],
            colModel:[
                {name:'Icon', index:'Icon', align:"center", width:32, search: false, editable: false, sortable:false, fixed:true},
                {name:'ChanId', editable: true, width:50, sorttype:"int", hidden:true, jsonmap: 'ChanId'},
                {name:'ChanNum', search: true, editable: true, width:70, sorttype:"int", jsonmap: 'ChanNum'},
                {name:'CallSign', search: true, editable: true, width:90, sorttype:"text", jsonmap: 'CallSign'},
                {name:'ChannelName', search: true, editable: true, width:90, sorttype:"text", jsonmap: 'ChannelName'},
                {name:'XMLTVID', search: true, editable: true, width:120,  sortable:false, jsonmap: 'XMLTVID'},
                {name:'Visible', search: false, editable: true, width:40, align:"center", sorttype:"bool", jsonmap: 'Visible', formatter:'checkbox',edittype:"checkbox"},
                {name:'IconURL', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'IconURL'},
                {name:'MplexId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'MplexId'},
                {name:'TransportId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'TransportId'},
                {name:'ServiceId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ServiceId'},
                {name:'NetworkId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'NetworkId'},
                {name:'ATSCMajorChan', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMajorChan'},
                {name:'ATSCMinorChan', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMinorChan'},
                {name:'Format', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Format'},
                {name:'Modulation', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Modulation'},
                {name:'Frequency', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Frequency'},
                {name:'FrequencyId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyId'},
                {name:'FrequencyTable', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyTable'},
                {name:'FineTune', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FineTune'},
                {name:'SIStandard', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SIStandard'},
                {name:'ChanFilters', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ChanFilters'},
                {name:'SourceId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SourceId'},
                {name:'InputId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'InputId'},
                {name:'CommFree', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'CommFree'},
                {name:'UseEIT', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'UseEIT'},
                {name:'DefaultAuth', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'DefaultAuth'}
            ],
            jsonReader: {
            root:"ChannelInfoList.ChannelInfos",
            page:"ChannelInfoList.CurrentPage",
            total:"ChannelInfoList.TotalPages",
            records:"ChannelInfoList.TotalAvailable",
            cell:"ChannelInfos",
            id : "ChanId",
            repeatitems: false
            },
            afterInsertRow: function (rowid, aData) {
                var html;
                if (aData.IconURL.length > 0)
                    html = '<img src="' + aData.IconURL + '" alt="" height="32" width="32" >';
                else
                    html = '<img src="/images/blank.gif" alt="" height="32" width="32" >';

                jQuery('#channels').setCell(rowid, 'Icon', html);
            },
            rowNum:20,
            multiselect: true,
            rowList:[10,20,30,50],
            pager: '#pager',
            defaultSearch: 'cn',
            ignoreCase: true,
            sortname: 'ChanNum',
            viewrecords: true,
            sortorder: "desc",
            loadonce: true,
            autoWidth: true,
            width: 820,
            height: 442
        });

        /* Add a search toolbar */

        jQuery("#channels").jqGrid('filterToolbar',{ searchOnEnter: false, defaultSearch: "cn" });

        /* Resize the grid if the window width changes */

        $(window).bind('resize', function() {
            $("#channels").setGridWidth($(window).width() - 55);
            $("#channels").setGridHeight($(window).height() - 205);
        }).trigger('resize');

        /* Sort ascending by Channel Number on load */

        $("#channels").setGridParam({sortname:'ChanNum', sortorder:'asc'}).trigger('reloadGrid');

        /* Initialize the Popup Menu */

        $("#channels").contextMenu('channelmenu', {
                bindings: {
                    'editopt': function(t) {
                        MythChannelEditor.EditChannel();
                    },
                    'del': function(t) {
                        MythChannelEditor.PromptToDeleteChannel();
                    }
                },
                onContextMenu : function(event, menu) {
                    var rowId = $(event.target).parent("tr").attr("id")
                    var grid = $("#channels");
                    if (rowId != $('#channels').getGridParam('selrow'))
                        grid.setSelection(rowId);
                    return true;
                }
        });
    }

   /*!
    * \fn InitSourceList
    * \private
    */
    var InitSourceList = function ()
    {
        $("#sourceSelect").html("Guide Data Source: <select id='sourceList' "
            + "onChange='javascript:MythChannelEditor.ReloadChannelGrid()'>"
            + GetGuideSourceList() + "</select>");
    }

   /*!
    * \fn EditSelectedChannel
    * \private
    */
    var EditSelectedChannel = function ()
    {
        loadEditWindow("/setup/channeleditor-channeldetail.html");
        var row = $('#channels').getGridParam('selrow');
        var rowdata = $("#channels").jqGrid('getRowData', row);

        /* Basic */

        $("#channelDetailSettingChanNum").val(rowdata.ChanNum);
        $("#channelDetailSettingChannelName").val(rowdata.ChannelName);
        $("#channelDetailSettingCallSign").val(rowdata.CallSign);
        InitXMLTVIdList();
        $("#channelDetailSettingXMLTVID").val(rowdata.XMLTVID);
        $("#channelDetailSettingIconURL").val(rowdata.IconURL);
        var preview = $("#channelDetailSettingIconPreview");
        if (rowdata.IconURL.length > 0) {
            preview.attr('src', rowdata.IconURL);
        }
        else
            preview.attr('src', '/images/blank.gif');

        if (rowdata.Visible == "Yes") {
            $("#channelDetailSettingVisible").attr('checked', true);
        }
        if (rowdata.UseEIT == "Yes")
            $("#channelDetailSettingUseEIT").attr('checked', true);

        /* Advanced */

        var sourceid = rowdata.SourceId;

        $("#channelDetailSettingFrequencyId").val(rowdata.FrequencyId);
        $("#channelDetailSettingMplexId").html(InitVideoMultiplexSelect(sourceid));
        $("#mplexList").val(rowdata.MplexId);
        $("#channelDetailSettingServiceId").val(rowdata.ServiceId);
        $("#channelDetailSettingDefaultAuthority").val(rowdata.DefaultAuth);
        $("#channelDetailSettingATSCMajorChannel").val(rowdata.ATSCMajorChan);
        $("#channelDetailSettingATSCMinorChannel").val(rowdata.ATSCMinorChan);

        /* Expert */

        $("#channelDetailSettingChanId").html(rowdata.ChanId);
        $("#channelDetailSettingSourceId").html(sourceid);
        $("#channelDetailSettingModulation").html(rowdata.Modulation)
        $("#channelDetailSettingFrequency").html(rowdata.Frequency)
        $("#channelDetailSettingSIStandard").html(rowdata.SIStandard)
        $("#channelDetailSettingFormat").html(rowdata.Format);

        $("#edit").dialog({
            modal: true,
            width: 800,
            height: 620,
            'title': 'Edit Channel',
            closeOnEscape: false,
            buttons: {
            'Save': function() { MythChannelEditor.SaveChannelEdits(); },
            'Cancel': function() { $(this).dialog('close'); }
        }});

        $("#channelsettings").accordion();

        $("#edit").dialog("open");
    }

   /*!
    * \fn EditMultiChannel
    * \private
    */
    var EditMultiChannel = function ()
    {
        loadEditWindow("/setup/channeleditor-channeldetail-multi.html");
        var rows = $('#channels').getGridParam('selarrrow');

        var rowinfo = GetMultiRowDescription();

        $("#channeldetailtable").html(rowinfo);

        $("#edit").dialog({
            modal: true,
            width: 800,
            height: 620,
            'title': 'Edit Multiple Channels',
            closeOnEscape: false,
            buttons: {
            'Save': function() { showConfirm('Editing multiple channels at once should be done with great care.  Do you want to continue?  This cannot be undone.', MythChannelEditor.SaveMultiChannelEdits); },
            'Cancel': function() { $(this).dialog('close'); }
        }});

        $("#multichannelsettings").accordion();
    }

   /*!
    * \fn GetMultiRowDescription
    * \private
    */
    var GetMultiRowDescription = function ()
    {
        var result = '';
        var rowArr = $('#channels').getGridParam('selarrrow');

        $.each(rowArr, function(i, value) {
            var rowdata = $("#channels").jqGrid('getRowData', value);
            result = result + "Affected ChanId:" + rowdata.ChanId + "(" + rowdata.ChannelName + ")" + "<br>";
        });

        return result;
    }

   /*!
    * \fn MultiChannelEditTextReplace
    * \private
    */
    var MultiChannelEditTextReplace = function ()
    {
        var template = $("#multiChannelRenameTemplate").val();

        template = template.replace("%ChanId%", rowdata.ChanId);
        template = template.replace("%ChanNum%", rowdata.ChanNum);
        template = template.replace("%ChanName%", rowdata.ChannelName);
        template = template.replace("%CallSign%", rowdata.CallSign);
        template = template.replace("%MplexId%", rowdata.MplexId);
        template = template.replace("%ServiceId%", rowdata.ServiceId);
        template = template.replace("%ATSCMajorChan%", rowdata.ATSCMajorChan);
        template = template.replace("%ATSCMinorChan%", rowdata.ATSCMinorChan);
        template = template.replace("%SourceId%", rowdata.SourceId);
        template = template.replace("%XMLTVID%", rowdata.XMLTVID);

        return template;
    }

   /*!
    * \fn UpdateChannelRow
    * \private
    */
    var UpdateChannelRow = function (rownum, rowdata)
    {
        if ($("#channels").jqGrid('setRowData', rownum, rowdata)) {
            if (UpdateDBChannelRow(rowdata))
                return true;
            else
                return false;
        }
        else {
            return false;
        }
    }

   /*!
    * \fn UpdateDBChannelRow
    * \private
    */
    var UpdateDBChannelRow = function (rowdata)
    {
        result = false;
        var visible = false;
        var useeit = false;

        if (rowdata.Visible == "Yes")
            visible = true;
        if (rowdata.UseEIT == "Yes")
            useeit = true;

        $.ajaxSetup({ async: false });
        $.post("/Channel/UpdateDBChannel",
            { MplexID: rowdata.MplexId, SourceID: rowdata.SourceId, ChannelID: rowdata.ChanId,
            CallSign: rowdata.CallSign, ChannelName: rowdata.ChannelName, ChannelNumber: rowdata.ChanNum,
            ServiceID: rowdata.ServiceId, ATSCMajorChannel: rowdata.ATSCMajorChan,
            ATSCMinorChannel: rowdata.ATSCMinorChan, UseEIT: useeit , visible: visible,
            FrequencyID: rowdata.FrequencyId, Icon: rowdata.IconURL, Format: rowdata.Format, XMLTVID: rowdata.XMLTVID,
            DefaultAuthority: rowdata.DefaultAuth},
            function(data) {
                if (data.bool == "true") {
                    $('#channels').trigger('reloadGrid');
                    result = true;
                }
                else
                    setErrorMessage("Channels update failed!");
            }, "json");
        $.ajaxSetup({ async: true });

        return result;
    }

   /*!
    * \fn GetGuideSourceList
    * \private
    */
    var GetGuideSourceList = function () {
        var result = '';

        $.ajaxSetup({ async: false });
        $.getJSON("/Channel/GetVideoSourceList", function(data) {
            $.each(data.VideoSourceList.VideoSources, function(i, value) {
                var sourcename = ''
                if (value.SourceName == '')
                    sourcename = "Source ID " + value.Id;
                else
                    sourcename = value.SourceName;
                result += "<option value='" + value.Id + "'>" + sourcename + "</option>";
            });
        });
        $.ajaxSetup({ async: true });

        return result;
    }

   /*!
    * \fn GetVideoMultiplexList
    * \private
    */
    var GetVideoMultiplexList = function (sourceid)
    {
        var result = '';

        $.ajaxSetup({ async: false });
        $.post("/Channel/GetVideoMultiplexList",
            { SourceID : sourceid },
            function(data) {
            $.each(data.VideoMultiplexList.VideoMultiplexes, function(i, value) {
                var label = '';
                if (value.TransportId > 0)
                    label = value.MplexId + " " + "(Transport: " + value.TransportId + ")";
                else
                    label = value.MplexId + " " + "(Frequency: " + value.Frequency + ")";
                result += "<option value='" + value.MplexId + "'>" + label + "</option>";
            });
        }, "json");
        $.ajaxSetup({ async: true });

        return result;
    }

   /*!
    * \fn InitVideoMultiplexSelect
    * \private
    */
    var InitVideoMultiplexSelect = function (sourceid)
    {
        var result = "<select id='mplexList'> "
            + GetVideoMultiplexList(sourceid) + "</select>";

        return result;
    }

   /*!
    * \fn InitXMLTVIdList
    * \private
    */
    var InitXMLTVIdList = function ()
    {
        var sourceid = $("#sourceList").val();
        var ids = new Array();
        var x = 0;
        $.ajaxSetup({ async: false });
        $.post("/Channel/GetXMLTVIdList", { SourceID : sourceid }, function(data) {
        $.each(data.StringList, function(i, value) {
                ids[x] = value;
                x++;
            });
        }, "json");

        $.ajaxSetup({ async: true });

        $( "#channelDetailSettingXMLTVID" ).autocomplete({ source: ids });
    }

   /*!
    * \fn ChooseNewChanIcon
    * \private
    */
    var ChooseNewChanIcon = function (url)
    {
        $("#channelDetailSettingIconURL").val(url);
        $("#channelDetailSettingIconPreview").attr('src',
            '/Content/GetFile?StorageGroup=ChannelIcons&FileName=' +
            basename(url));
    }

   /*!
    * \fn DownloadChannelIcon
    * \private
    */
    var DownloadChannelIcon = function (url)
    {
        $.post("/Content/DownloadFile",
            { URL: url, StorageGroup: "ChannelIcons" },
            function(data) {
                $("#channelDetailSettingIconPreview").attr('src',
                    '/Content/GetFile?StorageGroup=ChannelIcons&FileName=' +
                    basename(url));
                setStatusMessage("Channel Icon download succeeded!");
            }, "json").error(function() {
                setErrorMessage("Channel Icon download failed!");
            });
    }

   /*!
    * \var iconCount
    * \private
    */
    var iconCount = 0;
   /*!
    * \var iconRow
    * \private
    */
    var iconRow = "";

   /*!
    * \fn ChannelIconLink
    * \private
    */
    var ChannelIconLink = function (url, count)
    {
        return "<td class='iconGridCell'><a href='#' " +
            "onClick='javascript:MythChannelEditor.SelectChannelIcon(\"" + url + "\")'><img src='" +
            url + "' id='channelIcon_" + count + "' class='iconGridIcon'" +
            " onLoad='MythChannelEditor.ResizeChannelIcon(\"channelIcon_" + count + "\")'></a>" +
            "<br><div id='channelIcon_" + count + "_size'></div></td>";
    }

};

if (document.readyState == "complete")
    MythChannelEditor.Init();
else
    window.addEventListener("load", MythChannelEditor.Init);
window.addEventListener("unload", MythChannelEditor.Destructor);
