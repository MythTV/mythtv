cd xampp
start /min cmd /k "mysql\bin\mysqld --defaults-file=mysql\bin\my.ini --standalone"
cd ..
"bin/mythtv-setup.exe"
pause
