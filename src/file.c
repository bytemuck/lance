//
// Created by amelia on 9/27/26.
//

#include "file.h"

#include <stdio.h>
#include <stdlib.h>

const char* ReadFile(const char* path) {
    if (path == nullptr) {
        fprintf(stderr, "Error: No path provided\n");
        return nullptr;
    }

    FILE* file = fopen(path, "rb");
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

    char* const buffer = malloc((size_t) size + 1);
    if (buffer == nullptr) {
        fprintf(stderr, "Error: Could not allocate %ld bytes of memory for file buffer: '%s'\n", size, path);
        fclose(file);
        return nullptr;
    }

    const size_t bytes_read = fread(buffer, 1, (size_t) size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    return buffer;
}
