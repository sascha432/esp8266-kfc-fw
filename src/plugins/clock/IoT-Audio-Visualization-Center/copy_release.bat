@echo off
cd C:\Users\sascha\Documents\PlatformIO\Projects\IoT-Audio-Visualization-Center\bin\Release
del *.pdb /q
cd C:\Users\sascha\Documents\PlatformIO\Projects\kfc_fw\src\plugins\clock\IoT-Audio-Visualization-Center\bin
del * /q
xcopy /r C:\Users\sascha\Documents\PlatformIO\Projects\IoT-Audio-Visualization-Center\bin\Release C:\Users\sascha\Documents\PlatformIO\Projects\kfc_fw\src\plugins\clock\IoT-Audio-Visualization-Center\bin
