
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

/*
function embedConfigForm() {
    $("#form_buttons_top").click(submitConfigForm);
    $("#form_buttons_bottom").click(submitConfigForm);
}

embedConfigForm();
*/

