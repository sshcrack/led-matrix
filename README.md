# Led Matrix CPP
To install packages:
```shell
vcpkg install graphicsmagick curl spdlog cpr libxml2 restinio nlohmann_json fmt
cd uuid_v4
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="/usr/local" ..
sudo cmake --install .
```