# tcd

tcd is a hybrid digital voice transcoder for ham radio used by the new URF reflector.

## Introduction

This will build a new kind of hybrid transcoder that uses only software for vocoding digital voice streams used in DStar/DMR/YSF/NXDN *and* David Rowe's open-source Codec2 used in M17 as well as the open-source P25 vocoder, IMBE used in P25.

This is the only transcoder that will work with the [URF reflector](https://github.com/nostar/urfd).

This software is loosely based on LX3JL's **ambed**, but is easily different enough to be considered an entirely original work. Here are some major differences with ambed:

- *TCP Sockets* are used to communicate between the reflector and this transcoder. This guarantees that packets moving between the reflector and transcoder are never lost and the arrive at their destination in order.
- Each configured module has a dedicated encoding and decoding instance running on a different thread. This prevents overloading when processing multiple voice streams and provides the best possible performance for the reflector's clients.

## Constraints and Requirements

Only systemd-based operating systems are supported. Debian or Ubuntu is recommended. If you want to install this on a non-systemd based OS, you are on your own. Also, by default, *tcd* is built without gdb support.

## Download the repository

In the parent directory of you *urfd* repository:

```bash
git clone https://github.com/nostar/tcd.git
cd tcd
```

To be perfectly clear, the urfd reflector repository clone and this clone **must be in the same directory**. If your transcoder is a remote installation, you still need to `git clone https://github.com/nostar/urfd.git` even though you won't compile anything in the *urfd* repository. Both *tcd* and *urfd* repositories need to be in the same directory as several of the source files in *tcd* are symbolic links to the adjacent *urfd* reflector source code.

## Compiling and configuring *tcd*

 Copy the three configuration files to the working directory:

```bash
cp config/* .
```

Use your favorite text editor to edit the following files:
- *tcd.mk* defines some compile time options. Once you've set these options, do `make` to compile *tcd*. If you change `BINDIR`, you need to also change the `ExecStart` in your *tcd.service* file.

- *tcd.ini* defines run-time options. It is especially important that the `Modules` line for the tcd.ini file is exactly the same as the same line in the urfd.ini file! The `ServerAddress` is the url of the server. If the transcoder is local, this is usually `127.0.0.1` or `::1`. If the transcoder is remote, this is the IP address of the server. Suggested values for vocoder gains are provided.
- *tcd.service* is the systemd service file. You will need to modify the `ExecStart` line to successfully start *tcd* by specifying the path to your *tcd* executable and your tcd.ini file.

## Transcoding options

The md380_vocoder library for arm32 linux OS is located here:
https://github.com/nostar/md380_vocoder

The md380_vocoder library for all Intel/AMD 64-bit and ARM 64-bit platforms (including Android) is located here:
https://github.com/nostar/md380_vocoder_dynarmic

## Installing *tcd* when the transcoder is local

It is easiest to install and uninstall *tcd* using the ./radmin scripts in your urfd repo. If you want to do this manually:

```bash
sudo make install
sudo make uninstall
```

## Installing *tcd when the transcoder is remote

Use:
- `make` to compile *tcd*.
- `sudo make install` to install and run *tcd*.
- `sudo systemctl *something*` is used to manage the running *tcd*, where `*something*` might be `start`, `stop` or other verbs.
- `sudo journalctl -u tcd -f` to monitor the logs.
- `sudo make uninstall` to uninstall *tcd*.

When started, *tcd* will establish a TCP connection for each transcoded reflector module. If the TCP connection is lost, *tcd* will block until the connection is reestablished. A message will be printed every 10 seconds suggesting that the reflector needs to be restarted.
