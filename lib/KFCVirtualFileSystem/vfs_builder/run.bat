@echo off

set BASEDIR=C:\Users\sascha\Documents\PlatformIO\Projects\kfc_fw

cd "%BASEDIR%\lib\KFCVirtualFileSystem\vfs_builder"
.\vfs_builder.exe ..\..\..\data\webui\.mappings ..\..\..\data\webui.vfs
cd "%BASEDIR%"
