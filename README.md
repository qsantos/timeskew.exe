# timeskew.exe

`timeskew.exe` lets you accelerate the system time for specific programs.
This project is the Windows equivalent of [timeskew](https://github.com/vi/timeskew),
which works on Linux (using `LD_PRELOAD`).

Compare the output of:

```
> python test.py
```

and:

```
> SET TIMESKEW="10 1" && timeskew python test.py
```

This will continuously display the currently date and time every second.
The second one will go 10 times faster and quickly display times in the future.

If you do not have Python installed, you can use the Batch and PowerShell test scripts instead.

## Options

The behavior can be controlled with environment variables:

| Name             | Example        | Effect
| ----             | -------        | ------
| TIMESKEW         | `10 1`         | Set relative time speed to ×10
| TIMESKEW         | `1 2`          | Set relative time speed to ÷2
| TIMESKEW_LOGFILE | `-`            | Log calls to modified WINAPI functions to stdout
| TIMESKEW_LOGFILE | `timeskew.log` | Log calls to modified WINAPI functions to `timeskew.log` 
| TIMESKEW_PORT    | `40000`        | Listen on TCP port 40000 for updates to relative time speed

## Runtime control

To control the relative time speed while the application is running, start it with `TIMESKEW_PORT` set to a free port number.
Then, assuming Nmap is installed, run:

```
> ncat -v 127.0.0.1 40000
10 1
```

Instead of Nmap/Ncat, you can use telnet, although it is not installed on Windows by default anymore.

## How it works

This project uses [Detours](https://github.com/microsoft/Detours/),
which is similar to `LD_PRELOAD` on the (at least for the WINAPI).
To see how _this_ works, look at the [corresponding documentation](https://github.com/microsoft/Detours/wiki/OverviewInterception).

`timeskew.exe` uses Detours to intercepts calls to all WINAPI functions that involve time (that I know of).
To simulation the acceleration of time,
inputs (e.g. for `Sleep`) are reduced proportionally,
while outputs are increased proportionally (for instance `GetSystemTime`).
