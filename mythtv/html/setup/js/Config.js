
function drawInputDivOpen(item) {
    var result;

    result = "<div class=\"setting\" id=\"" +
        item.attr("value");
        + "_div\">\r\n";

    return result;
}

function drawInputDivClose(item) {
    return "</div>";
}

function drawItemDefault(item) {
    return "<div style=\"display:none; position:absolute; left:-4000px\" " +
        "id=\"" + item.attr("value") + "_default\">" +
        item.attr("default_data") + "</div>";
}

function drawTextInput(item) {
    var result = drawInputDivOpen(item);

    result += "<div class=\"setting_label\">" + item.attr("label") + "</div>";
    result += "<input name=\"" + item.attr("value") + "\" id=\"" +
        item.attr("value") + "_input\" type=\"text\" value=\"" +
        item.attr("data") + "\"/>";
    result += drawItemDefault(item);

    result += drawInputDivClose(item);
    return result;
}

function drawCheckBoxInput(item) {
    var result = drawInputDivOpen(item);

    result += "<div class=\"setting_label\">" + item.attr("label") + "</div>";
    result += "<input name=\"" + item.attr("value") + "\" id=\"" +
        item.attr("value") + "_input\" type=\"checkbox\" value=\"" +
        item.attr("data") + "\"/>";
    result += drawItemDefault(item);

    result += drawInputDivClose(item);
    return result;
}

function drawSelectInputOpen(item) {
    var result;

    result += "<div class=\"setting_label\">" + item.attr("label") + "</div>";
    result += "<select name=\"" + item.attr("value") + "\" id=\"" +
        item.attr("value") + "_input\">";

    return result;
}

function drawSelectInputClose(item) {
    return "</select>";
}

function drawSelectInput(item) {
    var result = drawInputDivOpen(item);
    result += drawSelectInputOpen(item);

    item.find('option').each(function()
    {
        result += "<option value=\"" + $(this).attr("data") + "\">" +
            $(this).attr("display") + "</option>";
    });

    result += drawSelectInputClose(item);
    result += drawItemDefault(item);
    result += drawInputDivClose(item);
    return result;
}

function drawIntegerRangeSelect(item) {
    var min = parseInt(item.attr("range_min"));
    var max = parseInt(item.attr("range_max"));
    var result = drawSelectInputOpen(item);

    var i = 0;
    for ( i = min; i <= max; i++)
    {
        result += "<option value=\"" + i + "\"";
        if (item.attr("data") == i)
            result += " selected";
        result += ">" + i + "</option>";
    }

    result += drawSelectInputClose(item);
    return result;
}

function drawIntegerRangeInput(item) {
    var min = parseInt(item.attr("range_min"));
    var max = parseInt(item.attr("range_max"));
    var diff = max - min;

    if (diff > 20)
        return drawTextInput(item);

    var result = drawInputDivOpen(item);
    result += drawIntegerRangeSelect(item);
    result += drawItemDefault(item);
    result += drawInputDivClose(item);
    return result;
}

function drawSetting(item) {
    var type = item.attr("data_type");
    if ((type == "string") ||
        (type == "integer") ||
        (type == "unsigned") ||
        (type == "float") ||
        (type == "combobox") ||
        (type == "ipaddress") ||
        (type == "timeofday") ||
        (type == "other") ||
        (type == "integer") ||
        (type == "integer"))
    {
        $("#content").append(drawTextInput(item));
    }
    else if (item.attr("data_type") == "checkbox")
    {
        $("#content").append(drawCheckBoxInput(item));
    }
    else if (item.attr("data_type") == "integer_range")
    {
        $("#content").append(drawIntegerRangeInput(item));
    }
    else if ((type == "select") ||
             (type == "localipaddress") ||
             (type == "frequency_table") ||
             (type == "tvformat"))
    {
        $("#content").append(drawSelectInput(item));
    }
    else
    {
        $("#content").append("<li>UNKNOWN Input Type: " +
            item.attr("label"));
    }
}

function drawGroup(item, level) {
    $("#content").append("<div class=\"group\" id=\"" +
            item.attr("unique_label") + "\">");

    if ((item.attr("human_label") != "undefined") &&
        (item.attr("human_label") != ""))
    {
        $("#content").append("<h" + level + " class=\"config\">" +
            item.attr("human_label") + "</h" + level + ">");
    }

    item.find("setting").each(function() { drawSetting($(this)); });
    item.find("group").each(function() { drawGroup($(this), level + 1); });

    $("#content").append("</div>");
}

function drawHeader(xml, form) {
    var html;

    html = "<div class=\"config\">"
        + " <h1 class=\"config\">MythTV Configuration</h1>"
        + " <form id=\"config_form\" action=\"/" + form
            + "/Save\" method=\"POST\">"
        + " <div class=\"form_buttons_top\" id=\"form_buttons_top\">"
        + "   <input type=\"submit\" value=\"Save Changes\" />"
        + " </div>";

    $("#content").html(html);
}

function drawFooter(xml) {
    var html;

    html = " <div class=\"form_buttons_top\" id=\"form_buttons_top\">"
        + "   <input type=\"submit\" value=\"Save Changes\" />"
        + "  </div>"
        + " </form>"
        + "</div>";

    $("#content").append(html);
}

function drawForm(xml) {
    $(xml).find("group").each(function() { drawGroup($(this), 2); });
    drawFooter(xml);
    embedForm();
}

function configDrawDatabaseForm(xml) {
    drawHeader(xml, "Config/Database");
    drawForm(xml);
}

function configDrawGeneralForm(xml) {
    drawHeader(xml, "Config/General");
    drawForm(xml);
}

function submitForm(event) {
    event.preventDefault();

    var data = $("#config_form").serialize();
    var url = $("#config_form").attr("action");

    // FIXME, this should be a POST
    var html = $.ajax({
        type: 'GET',
        url: url,
        data: data,
        async: false
    }).responseText;

    $("#content").html(html);

    // FIXME, may need to modify the page again here if the result contains another form
}

function embedForm() {
    $("#form_buttons_top").click(submitForm);
    $("#form_buttons_bottom").click(submitForm);
}

embedForm();

