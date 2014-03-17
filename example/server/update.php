<?
// update.php
// ============================
// Simple coded by rageworx@gmail.com

// First, get a string from HTTP/GET in URL.
$clientversionstr = $_GET["curversion"];

// Actually you may need to comapre with client version.
if ( $clientversionstr )
{
	// When it matches, just return DOS-TEXT style informations to client.
	// auto upgrader will be get this information.
    echo "URL=http://www.noterealserver.com/update/20140317/\r\n";
    echo "FILES=update.inf,updatepack20140317.dat,update.bat\r\n";
    echo "KILLS=test.exe\r\n";
    echo "EXECUTE=update.bat\r\n";
}
else
{
    echo "ERROR: NO PARAMETER\r\n";
}
?>
