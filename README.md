# TileGame

View the public web-build [here](https://marth1nus.github.io/TileGame)

# Compile

### Start but cloning the repo

```sh
git clone https://github.com/Marth1nus/TileGame
cd TileGame
```

### For a Native-Platform build (Linux, Mac, or Windows)

```sh
cmake --preset native
cmake --build --preset native
```

This will build for the **current development platform**.\
On **Linux** it will build for **Linux** and so on.

**Note** that on **Windows** it will try to build using **Ninja** which will only work when using **Developer PowerShell for VS 2022** or a shell with the associated **environment variables for Visual Studio**. To test if it will work try running `cl.exe` to see if it is available.

### For a Windows-Platform build

```sh
cmake --preset windows
cmake --build --preset windows
```

This will build using the **visual studio generator** so it can be done on any windows environment.

### For a Web-Platform build

```sh
cmake --preset emscripten
cmake --build --preset emscripten
```

This will build using **Emscripten** for **WASM**.

**Note**: Make sure that the `EMSDK` environment variable is available as it is used to find the toolchain file. The preset is disabled if the variable is not present.

# Support Goals

- **OpenGL API versions**:
  - OpenGL ES3
  - WebGL2
- **Platforms**:
  - Windows
  - Linux
  - Emscripten (web)
