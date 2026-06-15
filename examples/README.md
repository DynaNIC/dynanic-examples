# How to build and run examples

1. Create and enter build directory:
```bash
mkdir build
cd build
```

2. Compile with CMake:
```bash
cmake ../
make
```

3. Enter directory with example application of your choice and run it with elevated
privileges:

```bash
cd <app-name>
sudo ./<app-name> -- -h
```
