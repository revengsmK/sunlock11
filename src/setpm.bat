@echo off

takeown /f %systemroot%\System32\SettingsEnvironment.Desktop.dll /a
echo.
echo.
icacls %systemroot%\System32\SettingsEnvironment.Desktop.dll /grant Administrators:F
echo.

pause
exit