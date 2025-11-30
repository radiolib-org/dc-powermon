# dc-powermon
Raspberry Pi commandline software for reading out voltage and current measurements from INA219 . It is written primarily as RadioLib development tool, to be used on test setups and for hardware-in-the-loop testing.

## Dependencies

* cmake >= 3.18

## Building

Simply call the `build.sh` script.

## Usage

Start the program by calling `./build/dc-powermon`. Check the helptext `./build/dc-powermon --help` for all options. When called without arguments, it will assume default values which match [RadioHAT Rev. C](https://github.com/radiolib-org/RadioHAT).

## TODO list

In order of priorities:

* implement sampling rate control and/or decimation
* export timeseries in some reusable format
* allow control and data readout via sockets
