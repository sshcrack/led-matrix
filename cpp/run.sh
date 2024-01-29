echo Building
make -C build
echo Running
sudo ./build/main --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-slowdown-gpio 4