#Score-P perf energy consumption Counter

##Compilation and Installation

###Prerequisites

To compile this plugin, you need:

* C compiler

* CMake

* Score-P (or VampirTrace)

* A recent Linux kernel (`2.6.31+`) with activated tracing and the kernel headers

###Building

1. Create a build directory

        mkdir build
        cd build

2. Invoke CMake

    Specify the Score-P (and/or VampirTrace) directory if it is not in the default path with
    `-DSCOREP_DIR=<PATH>` (respectivly `-DVT_INC=<PATH>`). The plugin will use alternatively the
    environment variables `SCOREP_DIR` (and `VT_DIR`), e.g.

        cmake .. -DSCOREP_DIR=/PATH/TO/scorep

    or (for VampirTrace)

        cmake .. -DVT_DIR=/PATH/TO/vampirtrace

3. Invoke make

        make

4. Copy it to a location listed in `LD_LIBRARY_PATH` or add current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

##Usage

###Score-P

To use this plugin you have to add it to the `SCOREP_METRIC_PLUGINS` variable, e.g.:

    export SCOREP_METRIC_PLUGINS=perf_plugin

Afterwards you can select the metrics you want to measure from the list of the available metrics
using

    export SCOREP_METRIC_PERF_PLUGIN=<metric name>

See below for a list of available metrics.

 
###Available metrics

|  Name                         |                                   estimation fomula                                        |
|-------------------------------| -------------------------------------------------------------------------------------------|
|  energy-thread                |  1.602416e-9*instructions-9.779874e-11*cycles+6.437730e-08*cache_misses+2.418160e+03       |
|  power-energy-cores           |  RAPL*scale                                                                                |
|  ......                       |  ......                                                                    

E.g. 

    export SCOREP_METRIC_PERF_PLUGIN=power-energy-cores

###If anything fails

1. Check whether the plugin library can be loaded from the `LD_LIBRARY_PATH`.

2. If your kernel headers do not provide the file `include/linux/perf_events.h` please make sure you
    are using at least Linux `2.6.31`.

3. Write a mail to the author.

* Xiaosha Xing (xiaosha.xing at tu-dresden dot de)
