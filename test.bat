@echo off
:loop
echo %date% %time%
REM Somehow, timeout is not affected by timeskew
timeout /t 1 /nobreak > NUL
goto loop
