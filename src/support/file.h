//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_FILE_H
#define LANCE_FILE_H

#include <stdbool.h>

// Reads a whole file into a NUL-terminated heap buffer (release with free).
const char* ReadFile(const char* path);

// Path helpers. Every returned string is heap-allocated (release with free).
bool PathExists(const char* path);
char* CopyString(const char* text);
char* JoinPath(const char* directory, const char* name);
char* DirectoryOf(const char* path);

// Absolute path with symlinks and `..` resolved, or a copy of `path` if it
// can't be resolved. Used to recognise the same file reached two ways.
char* CanonicalPath(const char* path);

// Directory containing the running executable, falling back to the directory
// of argv[0] when the platform cannot tell.
char* ExecutableDirectory(const char* argv0);

#endif //LANCE_FILE_H
