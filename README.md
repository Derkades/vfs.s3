# Kodi S3 VFS

## Install AWS SDK

```
git clone --recurse-submodules https://github.com/aws/aws-sdk-cpp
cd aws-sdk-cpp
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_ONLY="s3"
cmake --build .
cmake --install .
```

## Compiling Addon
```
cmake .
make
sudo make install
```

or as single command:
```
cmake . && make && sudo make install
```

## Run Kodi with our libs debug logging
```
export LD_LIBRARY_PATH=/usr/lib:/usr/lib64:/usr/local/lib:/usr/local/lib64
clear; kodi --debug --logging=console
```

## Useful links
 * Other VFS addons https://github.com/xbmc/vfs.libarchive https://github.com/xbmc/vfs.sftp
 * Addon cmake doc https://alwinesch.github.io/group__cpp__cmake.html
 * VFS addon doc https://alwinesch.github.io/group__cpp__kodi__addon__vfs.html
 * Kodi linux compilation guide https://github.com/xbmc/xbmc/blob/master/docs/README.Linux.md
