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
- `test/strutiltest.cpp`: `_strutil.h` and `utfconvert.h` - every code point between UTF-8, UTF-16 and UTF-32, what is
  made of what is no UTF, encodings by byte order mark and by an XML document's first character, the C string
  functions, printf to a string, the reading of numbers.
- `test/containertest.cpp`: `_bitutil.h`, `_simple_bitvec` ( `_simpbv.h` ) and `LogArray` ( `_logarray.h` ), each against
  the standard library under random operations, the elements counted.
- `test/segarraytest.cpp`: `SegArray`, `SegArrayRotatingBuffer` and `SegArrayView` ( `segarray.h` ), likewise.
- `test/iotest.cpp`: `MemFile` and `MemStream` ( `memfile.h` ), the file functions of `_compat.h` / `_compat.inl`, and
  `FileObj` and `FileMappingObj` ( `_fdobjs.h` ).

They are what invalid, xmlp, lexang and dgraph use of bienutil.

Within another project they are built when it sets `BIENUTIL_BUILD_TESTS`. They run clean under
`-fsanitize=address,undefined`. Error paths are tested in Release builds: in Debug, `Assert()` breaks before the throw.
