#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <unistd.h>
#endif

const char *ReadFile(const char *path) {
	if (path == nullptr) {
		fprintf(stderr, "Error: No path provided\n");
		return nullptr;
	}

	FILE *file = fopen(path, "rb");
	if (file == nullptr) {
		fprintf(stderr, "Error: Could not open file: '%s'\n", path);
		return nullptr;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error: Could not seek to end of file: '%s'\n", path);
		fclose(file);
		return nullptr;
	}

	const long size = ftell(file);
	if (size < 0) {
		fprintf(stderr, "Error: Could not determine file size: '%s'\n", path);
		fclose(file);
		return nullptr;
	}

	rewind(file);

	char *const buffer = malloc((size_t) size + 1);
	if (buffer == nullptr) {
		fprintf(stderr, "Error: Could not allocate %ld bytes of memory for file buffer: '%s'\n", size, path);
		fclose(file);
		return nullptr;
	}

	const size_t bytes_read = fread(buffer, 1, (size_t) size, file);
	buffer[bytes_read]		= '\0';
	fclose(file);

	return buffer;
}

bool PathExists(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file)
		return false;
	fclose(file);
	return true;
}

char *CopyString(const char *text) {
	const size_t length = strlen(text);
	char		*copy	= malloc(length + 1);
	if (copy)
		memcpy(copy, text, length + 1);
	return copy;
}

char *JoinPath(const char *directory, const char *name) {
	const size_t directoryLength = strlen(directory);
	const size_t nameLength		 = strlen(name);
	const bool	 needsSeparator	 = directoryLength > 0 && directory[directoryLength - 1] != '/';
	char		*path			 = malloc(directoryLength + (needsSeparator ? 1 : 0) + nameLength + 1);
	if (!path)
		return nullptr;

	memcpy(path, directory, directoryLength);
	size_t offset = directoryLength;
	if (needsSeparator)
		path[offset++] = '/';
	memcpy(path + offset, name, nameLength + 1);
	return path;
}

char *DirectoryOf(const char *path) {
	const char *separator = strrchr(path, '/');
	if (!separator)
		return CopyString(".");
	if (separator == path)
		return CopyString("/");

	const size_t length	   = (size_t) (separator - path);
	char		*directory = malloc(length + 1);
	if (directory) {
		memcpy(directory, path, length);
		directory[length] = '\0';
	}
	return directory;
}

char *CanonicalPath(const char *path) {
#ifdef __linux__
	char *resolved = realpath(path, nullptr);
	if (resolved)
		return resolved;
#endif
	return CopyString(path);
}

char *ExecutableDirectory(const char *argv0) {
#ifdef __linux__
	char		  buffer[4096];
	const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length > 0) {
		buffer[length] = '\0';
		return DirectoryOf(buffer);
	}
#endif
	return argv0 ? DirectoryOf(argv0) : CopyString(".");
}
