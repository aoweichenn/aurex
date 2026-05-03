#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct M0SourceSet {
    char *paths[256];
    int32_t count;
} M0SourceSet;

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

static int32_t m0_runtime_c_string_len(const uint8_t *text) {
    int32_t len = 0;
    if (text == NULL) {
        return 0;
    }
    while (text[len] != 0) {
        ++len;
    }
    return len;
}

static M0FileBuffer m0_runtime_alloc_string(int32_t len) {
    M0FileBuffer result = {NULL, 0};
    uint8_t *data = (uint8_t *)malloc((size_t)len + 1);
    if (data == NULL) {
        return result;
    }
    data[len] = 0;
    result.data = data;
    result.len = len;
    return result;
}

static bool m0_runtime_is_path_byte(uint8_t ch) {
    return (ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_';
}

static int32_t m0_runtime_module_suffix_len(const uint8_t *source, int32_t begin, int32_t end) {
    int32_t len = 3;
    bool saw_segment_byte = false;
    for (int32_t i = begin; i < end; ++i) {
        const uint8_t ch = source[i];
        if (m0_runtime_is_path_byte(ch)) {
            ++len;
            saw_segment_byte = true;
        } else if (ch == '.' && saw_segment_byte) {
            ++len;
            saw_segment_byte = false;
        }
    }
    return len;
}

static int32_t m0_runtime_write_module_suffix(uint8_t *out, const uint8_t *source, int32_t begin, int32_t end) {
    int32_t at = 0;
    bool saw_segment_byte = false;
    for (int32_t i = begin; i < end; ++i) {
        const uint8_t ch = source[i];
        if (m0_runtime_is_path_byte(ch)) {
            out[at++] = ch;
            saw_segment_byte = true;
        } else if (ch == '.' && saw_segment_byte) {
            out[at++] = '/';
            saw_segment_byte = false;
        }
    }
    out[at++] = '.';
    out[at++] = 'a';
    out[at++] = 'x';
    return at;
}

M0SourceSet *m0_runtime_source_set_create(void) {
    return (M0SourceSet *)calloc(1, sizeof(M0SourceSet));
}

bool m0_runtime_source_set_add(M0SourceSet *set, const uint8_t *path) {
    if (set == NULL || path == NULL) {
        return false;
    }
    for (int32_t i = 0; i < set->count; ++i) {
        if (strcmp(set->paths[i], (const char *)path) == 0) {
            return false;
        }
    }
    if (set->count >= 256) {
        return false;
    }
    const size_t len = strlen((const char *)path);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, path, len + 1);
    set->paths[set->count++] = copy;
    return true;
}

void m0_runtime_source_set_destroy(M0SourceSet *set) {
    if (set == NULL) {
        return;
    }
    for (int32_t i = 0; i < set->count; ++i) {
        free(set->paths[i]);
    }
    free(set);
}

M0FileBuffer m0_runtime_import_path(const uint8_t *import_root, const uint8_t *source, int32_t begin, int32_t end) {
    const int32_t root_len = m0_runtime_c_string_len(import_root);
    const int32_t suffix_len = m0_runtime_module_suffix_len(source, begin, end);
    M0FileBuffer result = m0_runtime_alloc_string(root_len + suffix_len);
    if (result.data == NULL) {
        return result;
    }
    if (root_len > 0) {
        memcpy(result.data, import_root, (size_t)root_len);
    }
    const int32_t written = m0_runtime_write_module_suffix(result.data + root_len, source, begin, end);
    result.len = root_len + written;
    result.data[result.len] = 0;
    return result;
}

M0FileBuffer m0_runtime_module_import_root(const uint8_t *input_path, const uint8_t *source, int32_t begin, int32_t end) {
    M0FileBuffer suffix = m0_runtime_import_path((const uint8_t *)"", source, begin, end);
    M0FileBuffer result = {NULL, 0};
    if (suffix.data == NULL || input_path == NULL) {
        m0_runtime_free_file(suffix);
        return result;
    }

    const int32_t input_len = m0_runtime_c_string_len(input_path);
    if (input_len < suffix.len || memcmp(input_path + input_len - suffix.len, suffix.data, (size_t)suffix.len) != 0) {
        m0_runtime_free_file(suffix);
        result = m0_runtime_alloc_string(0);
        return result;
    }

    result = m0_runtime_alloc_string(input_len - suffix.len);
    if (result.data != NULL && result.len > 0) {
        memcpy(result.data, input_path, (size_t)result.len);
    }
    m0_runtime_free_file(suffix);
    return result;
}

M0WriteResult m0_runtime_write_c_string(const uint8_t *path, const uint8_t *data) {
    M0WriteResult result = {false, 0};
    FILE *file = fopen((const char *)path, "wb");
    if (file == NULL) {
        return result;
    }

    const int32_t len = m0_runtime_c_string_len(data);

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
    const int32_t len = m0_runtime_c_string_len(text);
    return fwrite(text, 1, (size_t)len, file) == (size_t)len;
}

bool m0_runtime_output_write_source_range(FILE *file, const uint8_t *source, int32_t begin, int32_t end) {
    if (file == NULL || source == NULL || begin < 0 || end < begin) {
        return false;
    }
    const size_t len = (size_t)(end - begin);
    return fwrite(source + begin, 1, len, file) == len;
}

bool m0_runtime_output_write_i32(FILE *file, int32_t value) {
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

bool m0_runtime_output_close(FILE *file) {
    if (file == NULL) {
        return false;
    }
    return fclose(file) == 0;
}
