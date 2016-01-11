copy /Y %1 "%TEMP%\uwin_uninstall.exe"
"%TEMP%\uwin_uninstall.exe" %2
del /F "%TEMP%\uwin_uninstall.exe"
ping -n 5 127.0.0.1 >nul
