# timeskew.exe

`timeskew.exe` lets you accelerate the system time for specific programs.
This project is the Windows equivalent of [timeskew](https://github.com/vi/timeskew),
which works on Linux (using `LD_PRELOAD`).

You can test this by running the following command:

```
> timeskew python test.py
```

This will continuously display the currently date and time every second.
Now, in another terminal, run (assuming you have installed Nmap):

```
> ncat -v 127.0.0.1 40000
10 1
```

This will make time go 10 times faster for the Python script.
The Python script should quickly start times in the future.
It will do so at a faster pace, since the period is correspondingly shortened.

If you do not have Python installed, you can use the Batch and PowerShell test scripts instead.
Instead of Nmap/Ncat, you can use telnet, although it is not installed on Windows by default.
