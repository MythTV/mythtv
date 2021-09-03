cd xampp
start /min cmd /k "mysql\bin\mysqld --defaults-file=mysql\bin\my.ini --standalone"
cd ..
TIMEOUT 1 /nobreak
start cmd /k "bin\mythbackend.exe"
TIMEOUT 16 /nobreak
"bin\mythfrontend.exe"  --disable-autodiscovery --noupnp
pause
