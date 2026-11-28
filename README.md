# bienutil
utility modules

## Tests

The headers are header-only; `CMakeLists.txt` gives them as the interface library `bienutil` and builds the unit tests in
`test/` ( Google test, fetched ):

```
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release && ctest --test-dir out/build/release
```

- `test/jsontest.cpp`: the JSON reader and writer ( `jsonstrm.h` ) and the values built on them ( `jsonobjs.h` ) - what is
  read and what is refused, the writer's output to the character, escaping, round trips of random values, the read
  cursor used directly, filters, files, `char8_t` / `char16_t` / `char32_t`.
- `test/strwrsvtest.cpp`: `StrWRsv` ( `strwrsv.h` ), the string every JSON string is kept in.

Within another project they are built when it sets `BIENUTIL_BUILD_TESTS`. They run clean under
`-fsanitize=address,undefined`. Error paths are tested in Release builds: in Debug, `Assert()` breaks before the throw.
