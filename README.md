# magic

This extension, Magic, allow you to examine files and determine their type, based on https://man7.org/linux/man-pages/man3/libmagic.3.html linux utility.


```sql
--- Install (once)
INSTALL magic FROM community;

--- Update (to check for updates
UPDATE EXTENSIONS (magic);

--- Load
LOAD magic;
```

Example, discover which mime_types is a given [remote] file[s]:
```sql
SELECT magic_mime(file), magic_type(file), file
    FROM glob('https://raw.githubusercontent.com/duckdb/duckdb/main/data/parquet-testing/adam_genotypes.parquet');
```

Example, read any file with autodetection (on the content or the name):
```sql
FROM read_any('https://raw.githubusercontent.com/duckdb/duckdb/main/data/parquet-testing/adam_genotypes.parquet');
```

This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

---

### Building

```
VCPKG_TOOLCHAIN_PATH='/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake' GEN=ninja make
-- or without ninja
VCPKG_TOOLCHAIN_PATH='/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake' make
```
