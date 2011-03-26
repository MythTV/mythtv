
function changePassword() {
    var result = false;
    var oldPassword = $("#oldPassword").val();
    var newPassword = $("#newPassword").val();

    $.post("/Myth/ChangePassword",
        { UserName: "admin",
          OldPassword: oldPassword,
          NewPassword: newPassword },
        function(data) {
            if (data.bool == "true")
                alert("Password successfully changed.");
            else
                alert("Error changing password, check backend logs for detailed information.");
        }, "json").error(function(data) {
            alert("Error changing password, check backend logs for detailed information.");
        });
}

