# SQLite3 Amalgamation

This folder should contain the SQLite3 amalgamation files:

- `sqlite3.c` - SQLite3 amalgamation source
- `sqlite3.h` - SQLite3 header file
- `sqlite3ext.h` - SQLite3 extension header (optional)

## Download Instructions

1. Visit [SQLite Download Page](https://www.sqlite.org/download.html)
2. Under "Source Code", download **sqlite-amalgamation-*.zip**
3. Extract `sqlite3.c` and `sqlite3.h` to this folder

## Compile Options

The following compile-time options are enabled in the project:

- `SQLITE_THREADSAFE=1` - Serialized threading mode
- `SQLITE_ENABLE_FTS5` - Full-text search support
- `SQLITE_ENABLE_JSON1` - JSON1 extension

## Version

Compatible with SQLite 3.40.0 or later.
