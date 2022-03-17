# nvidia-oobaml

NVIDIA OOB Active Monitoring and Logging

## Build
### Environment Setup
 
OS | Build Tool Package | Code Support Package
--- | --- | ---
Ubuntu 18.04 | g++<br>autoconf<br>autoconf-archive<br>pkg-config<br>libtool-bin<br>doxygen<br>graphviz | OpenBMC SDK<br>libgpu (NVIDIA Firmware OOB Module)
Cygwin | g++<br>autoconf<br>autoconf-archive<br>pkg-config<br>libtool-bin<br>doxygen<br>graphviz | OpenBMC SDK<br>libgpu (NVIDIA Firmware OOB Module)
|
 
OpenBMC SDK Installation Instructions: [link](https://github.com/openbmc/docs/blob/master/development/dev-environment.md#download-and-install-sdk)
 
Instead of setting up those manually, run following script inside *source code folder* will help,
``` shell
$ sudo scripts/setup_bldenv
```
>**NOTE: Cygwin packages need to be installed manually with [Cygwin setup utility](https://www.cygwin.com/setup-x86_64.exe)!**
 
### How to build
The general build steps are as below, inside *source code folder* and run,
```
$ meson builddir # configure 
$ ninja -C builddir # build
$ ninja -C builddir install # Optional
```


[Project Options](#tablebuildmode) are defined as below, any combinations of them are valid,
<a id="tablebuildmode"></a>
 
Mode | `Project options` | Possible Values | Description
--- | --- | --- | ---
API Stubs | `api_stubs` | [enabled, disabled, auto] | Build this modeule using stub APIs included in directory,
*src/stubs*
Sandbox | `sandbox_mode` | [enabled, disabled, auto] | Build this module for the host CPU architecture, so as to develop & debug & verify without burning code into BMC with real hardware.
Default Debug Log Level | `debug_log=n` | >=0, <=4 | Log level `n` definition is [here](#tabledbgloglevel).
 
 ### How to Clean
To clean build cache,
``` shell
$ ninja -C builddir clean
$ ninja -C builddir uninstall #optional
```
 
To completely clean the workspace,
``` shell
$ rm -rf builddir
```

 ### Enable Project Options
 These commands would work for clean projects. If already configured, refer [section](#enable-project-options-for-configured-projects)

 #### API Stubs
 ``` shell
 $ meson builddir -Dapi_stubs=enabled
 $ ninja -C builddir
 $ ninja -C builddir install # Optional
 ```

 #### Sandbox Mode
 ``` shell
 $ meson builddir -Dsandbox_mode=enabled
 $ ninja -C builddir
 $ ninja -C builddir install # Optional
 ```

 #### Debug Log Level
 ``` shell
 $ meson builddir -Ddebug_log={0,1,2,3,4}
 $ ninja -C builddir
 $ ninja -C builddir install # Optional
 ```
 ### Enable Project Options for Configured Projects
 If the project is already configured using,
 ``` shell
 $ meson builddir
 ```
 , user can use `meson configure` to view/edit project options.

The complete list of project options can be accessed,
``` shell
$ cd builddir
$ meson configure
Project options                Current Value                    Possible Values                  Description
  ---------------                -------------                    ---------------                  -----------
  api_stubs                      auto                             [enabled, disabled, auto]        Manager API stubs enablement
  debug_log_level                0                                >=0, <=4                         Enable debug log LEVEL [0|1|2|3|4]
  sandbox_mode                   enabled                          [enabled, disabled, auto]        Sandbox mode enablement
```

Options can be edited,
``` shell
$ cd builddir # If not already
$ meson configure -Dapi_stubs=enabled -Ddebug_log=1
```

### Debug Logging Level Control
Debug Log Level supports to be configured on both build time and runtime.
 
There are 5 logging levels supported,
<a id="tabledbgloglevel"></a>
 
Level | Index
--- | ---
disabled | 0
error | 1 (default)
warning | 2
debug | 3
info | 4
|
 
By default, the logging level is 1 - error. To change it before building, e.g. to 3 - debug,
``` shell
$ ./configure --enable-debug-log=3
$ make clean && make
```
 
To change the logging level during runtime, e.g. to 3 - debug,
``` shell
$ tools/log_cmd 3
```
>**NOTE: log_cmd could be installed into the target firmware filesystem.**

### Docker Image for Building & Debugging
:warning: **Not maintained!** May need extensive work to work as described. 

A docker image is used for code building and debugging. It needs to be created for the first time by,
``` shell
$ ./buildenv create
```
It creates an image named "bldenv:module" with all build support packages for this module.
 
After that, everytime to build code just start it by (*current folder* will be mounted into the container),
``` shell
$ ./buildenv
```
Or,
``` shell
$ ./buildenv start [src_dir] # src_dir is where you want to mount into the container, default is current folder.
```
It will start a container based on the image for code building by following the same steps listed [above](#how-to-build).
In this container, user could do '**ninja -C builddir install**' to install the code packages just like in real production environment, without messing up the OS.
By default, the container will keep it last states after exit. Do this to get a clean one,
``` shell
$ ./buildenv clean
$ ./buildenv
```
Or,
``` shell
$ ./buildenv clean
$ ./buildenv start [src_dir] # src_dir is where you want to mount into the container, default is current folder.
```