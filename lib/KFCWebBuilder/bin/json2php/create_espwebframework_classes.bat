@echo off
set CONFIG=./ESPWebFrameWork.json
set OUTPUT_DIR=../ESPWebFramework/JsonConfiguration/
echo Using "%CONFIG%" to create classes in "%OUTPUT_DIR%"
php json2php -v g:ewf -t file -o %OUTPUT_DIR% %CONFIG%
pause
