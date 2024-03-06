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

## Options

The behavior can be controlled with environment variables:

| Name             | Example        | Effect
| ----             | -------        | ------
| TIMESKEW         | `10 1`         | Set relative time speed to ×10
| TIMESKEW         | `1 2`          | Set relative time speed to ÷2
| TIMESKEW_LOGFILE | `-`            | Log calls to modified WINAPI functions to stdout
| TIMESKEW_LOGFILE | `timeskew.log` | Log calls to modified WINAPI functions to `timeskew.log` 
| TIMESKEW_PORT    | `40000`        | Listen on TCP port 40000 for updates to relative time speed

## How it works

This project uses [Detours](https://github.com/microsoft/Detours/),
which is similar to `LD_PRELOAD` on the (at least for the WINAPI).
To see how _this_ works, look at the [corresponding documentation](https://github.com/microsoft/Detours/wiki/OverviewInterception).

`timeskew.exe` uses Detours to intercepts calls to all WINAPI functions that involve time (that I know of).
To simulation the acceleration of time,
inputs (e.g. for `Sleep`) are reduced proportionally,
while outputs are increased proportionally (for instance `GetSystemTime`).
