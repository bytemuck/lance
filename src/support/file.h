#ifndef LANCE_FILE_H
#define LANCE_FILE_H

const char *ReadFile(const char *path);

bool  PathExists(const char *path);
char *CopyString(const char *text);
char *JoinPath(const char *directory, const char *name);
char *DirectoryOf(const char *path);

char *CanonicalPath(const char *path);

char *ExecutableDirectory(const char *argv0);

#endif // LANCE_FILE_H
