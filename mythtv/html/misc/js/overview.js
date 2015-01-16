
function transferComplete()
{
    console.log("Success");
    console.log(this.responseText);
}

function GetFrontendStatus(name, address)
{
    var url = "http://" + address + "/Frontend/GetStatus";
    console.log("GetFrontendStatus - " + url);
    $.ajax({url: url,
            dataType: "xml",
            success: function(data, textStatus, jqXHR) {
                console.log("Success");
                $xml = $( data );
                $stateStrings = $xml.find("String");
                //console.log($stateStrings);
                $stateStrings.each(function() {
                    if ($(this).find("Key").text() == "state")
                    {
                        $("#frontendStatus-" + name).html( $(this).find("Value").text() );
                        console.log($(this).find("Key").text() + " : " + $(this).find("Value").text());
                        return;
                    }
                });
            },
            error: function(jqXHR, textStatus, errorThrown) {
                console.log("Error: " + errorThrown);
            },
            complete: function() {
                console.log("Complete");
            }});
}

