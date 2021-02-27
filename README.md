**dbsdrt**

SDR software for the HackRF One

## Dependencies

```bash
$ sudo apt install cmake libncurses-dev libhackrf-dev libfftw3-dev 
```

## Build

```bash
$ git clone https://github.com/dbrentley/dbsdrt.git
$ cd dbsdrt
$ mkdir build && cd build
$ cmake ..
$ make
```

## Run

```bash
$ ./dbsdrt
```