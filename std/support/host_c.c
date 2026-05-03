#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Aurex std host-c backend support, ABI v0.
 *
 * Stable exported support symbols use the aurex_std_v0_* namespace. The
 * unversioned aurex_std_* wrappers are kept for older Stage1/selfhost slices
 * and should not be used by new std declarations.
 */

typedef struct AurexStdFileBuffer {
    uint8_t *data;
    int32_t len;
} AurexStdFileBuffer;

typedef struct AurexStdWriteResult {
    bool ok;
    int32_t written;
} AurexStdWriteResult;

AurexStdFileBuffer aurex_std_v0_read_file(const uint8_t *path) {
    AurexStdFileBuffer result = {NULL, 0};
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

void aurex_std_v0_free_file(AurexStdFileBuffer buffer) {
    free(buffer.data);
}

static int32_t aurex_std_host_c_string_len(const uint8_t *text) {
    int32_t len = 0;
    if (text == NULL) {
        return 0;
    }
    while (text[len] != 0) {
        ++len;
    }
    return len;
}

AurexStdWriteResult aurex_std_v0_write_text(const uint8_t *path, const uint8_t *data) {
    AurexStdWriteResult result = {false, 0};
    FILE *file = fopen((const char *)path, "wb");
    if (file == NULL) {
        return result;
    }

    const int32_t len = aurex_std_host_c_string_len(data);
    const size_t written = fwrite(data, 1, (size_t)len, file);
    const int close_result = fclose(file);
    result.ok = written == (size_t)len && close_result == 0;
    result.written = (int32_t)written;
    return result;
}

FILE *aurex_std_v0_output_open(const uint8_t *path) {
    return fopen((const char *)path, "wb");
}

bool aurex_std_v0_output_write_text(FILE *file, const uint8_t *text) {
    if (file == NULL) {
        return false;
    }
    const int32_t len = aurex_std_host_c_string_len(text);
    return fwrite(text, 1, (size_t)len, file) == (size_t)len;
}

bool aurex_std_v0_output_write_source_range(FILE *file, const uint8_t *source, int32_t begin, int32_t end) {
    if (file == NULL || source == NULL || begin < 0 || end < begin) {
        return false;
    }
    const size_t len = (size_t)(end - begin);
    return fwrite(source + begin, 1, len, file) == len;
}

bool aurex_std_v0_output_write_i32(FILE *file, int32_t value) {
    if (file == NULL) {
        return false;
    }
    char buffer[32];
    const int len = snprintf(buffer, sizeof(buffer), "%d", value);
    if (len < 0 || (size_t)len >= sizeof(buffer)) {
        return false;
    }
    return fwrite(buffer, 1, (size_t)len, file) == (size_t)len;
}

bool aurex_std_v0_output_close(FILE *file) {
    if (file == NULL) {
        return false;
    }
    return fclose(file) == 0;
}

AurexStdFileBuffer aurex_std_read_file(const uint8_t *path) {
    return aurex_std_v0_read_file(path);
}

void aurex_std_free_file(AurexStdFileBuffer buffer) {
    aurex_std_v0_free_file(buffer);
}

AurexStdWriteResult aurex_std_write_text(const uint8_t *path, const uint8_t *data) {
    return aurex_std_v0_write_text(path, data);
}

FILE *aurex_std_output_open(const uint8_t *path) {
    return aurex_std_v0_output_open(path);
}

bool aurex_std_output_write_text(FILE *file, const uint8_t *text) {
    return aurex_std_v0_output_write_text(file, text);
}

bool aurex_std_output_write_source_range(FILE *file, const uint8_t *source, int32_t begin, int32_t end) {
    return aurex_std_v0_output_write_source_range(file, source, begin, end);
}

bool aurex_std_output_write_i32(FILE *file, int32_t value) {
    return aurex_std_v0_output_write_i32(file, value);
}

bool aurex_std_output_close(FILE *file) {
    return aurex_std_v0_output_close(file);
}
