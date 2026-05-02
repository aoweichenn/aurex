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
