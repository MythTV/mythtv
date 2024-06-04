echo off

echo "1. Run Install Mythtv.cmd"
echo "2. Go to General->'Host Address and Network setup' (should be 127.0.0.1) and then exit and save."
echo "When exiting click do not fix"
echo "3. Run MythFrontend & Backend"
echo "config.xml is stored in C:\Users\%username%\AppData\Local\mythtv". Please enter your backend ip address/password if required.
echo "You may need to tweak the frontend video profiles under Setup->Video->playback and select opengl normal"

echo "Press ctrl-x to quit the command prompt if mythtv gets stuck closing"

echo "Ok?"
pause


robocopy share C:\ProgramData\ /E

if not exist xampp-portable-windows.zip (
    curl -kL "https://sourceforge.net/projects/xampp/files/XAMPP%20Windows/8.0.25/xampp-portable-windows-x64-8.0.25-0-VS16.zip/download" > xampp-portable-windows.zip
)

if not exist timezone_2021a_leaps.zip (
    curl -kL https://downloads.mysql.com/general/timezone_2021a_leaps.zip > timezone_2021a_leaps.zip
)

echo "copying files.."
tar -vxf xampp-portable-windows.zip
tar -vxf timezone_2021a_leaps.zip

robocopy timezone_2021a_leaps\ xampp\mysql\data\mysql\

rmdir timezone_2021a_leaps /s /q

cd xampp
start /min cmd /k "setup_xampp.bat"
TIMEOUT 5 /nobreak
start /min cmd /k "mysql\bin\mysqld --defaults-file=mysql\bin\my.ini --standalone"
TIMEOUT 7 /nobreak
echo "creating user"
"mysql\bin\mysql.exe" -u root --skip-password  -e "CREATE USER 'mythtv'@'localhost' IDENTIFIED BY 'mythtv';CREATE DATABASE mythconverg;GRANT ALL PRIVILEGES ON *.* TO 'mythtv'@'localhost';"
"mysql\bin\mysql.exe" -u root -e "SELECT user FROM mysql.user where user = 'mythtv';"

cd ..
TIMEOUT 1 /nobreak
"bin/mythbackend.exe"
TIMEOUT 2 /nobreak
"bin/mythtv-setup.exe"
"bin/mythtv-setup.exe"
