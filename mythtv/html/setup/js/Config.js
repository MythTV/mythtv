
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

