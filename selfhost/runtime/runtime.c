#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Placeholder runtime unit for the self-host track.
 *
 * M0 currently emits freestanding C for simple programs. As the self-hosted
 * compiler grows, explicit runtime services such as file IO, allocation, and
 * process exit shims should be added here instead of being hidden in codegen.
 */

typedef struct M0FileBuffer {
    uint8_t *data;
    int32_t len;
} M0FileBuffer;

typedef struct M0WriteResult {
    bool ok;
    int32_t written;
} M0WriteResult;

M0FileBuffer m0_runtime_read_file(const uint8_t *path) {
    M0FileBuffer result = {NULL, 0};
    FILE *file = fopen((const char *)path, "rb");
    if (file == NULL) {
        return result;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return result;
    }
    const long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return result;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return result;
    }

    uint8_t *data = (uint8_t *)malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(file);
        return result;
    }

    const size_t read_count = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(data);
        return result;
    }

    data[size] = 0;
    result.data = data;
    result.len = (int32_t)size;
    return result;
}

void m0_runtime_free_file(M0FileBuffer buffer) {
    free(buffer.data);
}

M0WriteResult m0_runtime_write_c_string(const uint8_t *path, const uint8_t *data) {
    M0WriteResult result = {false, 0};
    FILE *file = fopen((const char *)path, "wb");
    if (file == NULL) {
        return result;
    }

    int32_t len = 0;
    while (data[len] != 0) {
        ++len;
    }

    const size_t written = fwrite(data, 1, (size_t)len, file);
    const int close_result = fclose(file);
    result.ok = written == (size_t)len && close_result == 0;
    result.written = (int32_t)written;
    return result;
}

FILE *m0_runtime_output_open(const uint8_t *path) {
    return fopen((const char *)path, "wb");
}

bool m0_runtime_output_write_c_string(FILE *file, const uint8_t *text) {
    if (file == NULL) {
        return false;
    }
    int32_t len = 0;
    while (text[len] != 0) {
        ++len;
    }
    return fwrite(text, 1, (size_t)len, file) == (size_t)len;
}

bool m0_runtime_output_write_source_range(FILE *file, const uint8_t *source, int32_t begin, int32_t end) {
    if (file == NULL || source == NULL || begin < 0 || end < begin) {
        return false;
    }
    const size_t len = (size_t)(end - begin);
    return fwrite(source + begin, 1, len, file) == len;
}

bool m0_runtime_output_close(FILE *file) {
    if (file == NULL) {
        return false;
    }
    return fclose(file) == 0;
}
