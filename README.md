# magic


This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

---

This extension, Magic, allow you to examine files and determine their type, based on https://man7.org/linux/man-pages/man3/libmagic.3.html linux utility.

### Building

```
VCPKG_TOOLCHAIN_PATH='/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake' GEN=ninja make
-- or possibly without ninja
VCPKG_TOOLCHAIN_PATH='/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake' make
```
