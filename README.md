![GameInstance.com C++ audio](docs/logo.svg)

# C++ audio package - GameInstance.com

**audio** package contains C++ audio codecs.


## Building audio

Install **gcc** compiler and **make**, then create the work directory structure in which you'll clone
and build the **audio** repository and its dependencies: **stream** and  **basics**.

```
mkdir -p ~/gameinstance/BUILD/lib && mkdir -p ~/gameinstance/BUILD/bin
cd ~/gameinstance

git clone https://github.com/gameinstance/basics
cd basics
make install

git clone https://github.com/gameinstance/stream
cd stream
make install

git clone https://github.com/gameinstance/audio
cd audio
make install
```

## Building codec tool

Build the tool, then set the shared lib directory and run the flac decoder:

```
cd ~/gameinstance/audio/tools
make release
LD_LIBRARY_PATH=../../BUILD/lib/ ./BUILD/flac-decoder input.flac output.wav
```
