#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Aurex std host-c FFI support, ABI v0.
 *
 * This file is intentionally isolated under std/ffi/c so the temporary C FFI
 * bridge can be replaced without changing language-level std modules.
 */

typedef struct AurexStdFileBuffer {
    uint8_t *data;
    int32_t len;
} AurexStdFileBuffer;

typedef struct AurexStdWriteResult {
    bool ok;
    int32_t written;
} AurexStdWriteResult;

typedef struct AurexStdProcessOutput {
    int32_t status;
    uint8_t *stdout_data;
    int32_t stdout_len;
    uint8_t *stderr_data;
    int32_t stderr_len;
} AurexStdProcessOutput;

typedef struct AurexStdFileMetadata {
    int32_t exists;
    int32_t is_file;
    int32_t is_dir;
    int64_t size;
    int64_t modified_ns;
} AurexStdFileMetadata;

FILE *aurex_std_v0_stdout(void) {
    return stdout;
}

FILE *aurex_std_v0_stderr(void) {
    return stderr;
}

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

static int64_t aurex_std_host_c_modified_ns(const struct stat *info) {
#if defined(__APPLE__)
    return ((int64_t)info->st_mtimespec.tv_sec * 1000000000LL) + (int64_t)info->st_mtimespec.tv_nsec;
#else
    return ((int64_t)info->st_mtim.tv_sec * 1000000000LL) + (int64_t)info->st_mtim.tv_nsec;
#endif
}

bool aurex_std_v0_file_metadata(const uint8_t *path, AurexStdFileMetadata *output) {
    if (path == NULL || output == NULL) {
        return false;
    }

    output->exists = 0;
    output->is_file = 0;
    output->is_dir = 0;
    output->size = 0;
    output->modified_ns = 0;

    struct stat info;
    if (stat((const char *)path, &info) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return true;
        }
        return false;
    }

    output->exists = 1;
    output->is_file = S_ISREG(info.st_mode) ? 1 : 0;
    output->is_dir = S_ISDIR(info.st_mode) ? 1 : 0;
    output->size = info.st_size < 0 ? 0 : (int64_t)info.st_size;
    output->modified_ns = aurex_std_host_c_modified_ns(&info);
    return true;
}

static bool aurex_std_host_c_has_suffix(const char *name, const char *suffix) {
    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if (suffix_len == 0) {
        return true;
    }
    if (name_len < suffix_len) {
        return false;
    }
    return memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

static bool aurex_std_host_c_is_dot_entry(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static char *aurex_std_host_c_join_dir_entry(const char *dir_path, const char *entry_name) {
    const size_t dir_len = strlen(dir_path);
    const size_t entry_len = strlen(entry_name);
    const bool needs_separator = dir_len > 0 && dir_path[dir_len - 1] != '/';
    const size_t separator_len = needs_separator ? 1 : 0;
    const size_t total_len = dir_len + separator_len + entry_len;
    if (total_len < dir_len || total_len < entry_len || total_len == (size_t)-1) {
        return NULL;
    }

    char *joined = (char *)malloc(total_len + 1);
    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, dir_path, dir_len);
    size_t offset = dir_len;
    if (needs_separator) {
        joined[offset] = '/';
        ++offset;
    }
    memcpy(joined + offset, entry_name, entry_len);
    joined[total_len] = 0;
    return joined;
}

static bool aurex_std_host_c_is_regular_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

bool aurex_std_v0_directory_count_files_with_suffix(
    const uint8_t *path,
    const uint8_t *suffix,
    int32_t *output
) {
    if (path == NULL || suffix == NULL || output == NULL) {
        return false;
    }
    *output = 0;

    DIR *directory = opendir((const char *)path);
    if (directory == NULL) {
        return false;
    }

    int32_t count = 0;
    bool ok = true;
    while (true) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            ok = errno == 0;
            break;
        }
        if (aurex_std_host_c_is_dot_entry(entry->d_name) ||
            !aurex_std_host_c_has_suffix(entry->d_name, (const char *)suffix)) {
            continue;
        }

        char *entry_path = aurex_std_host_c_join_dir_entry((const char *)path, entry->d_name);
        if (entry_path == NULL) {
            ok = false;
            break;
        }
        const bool is_regular_file = aurex_std_host_c_is_regular_file(entry_path);
        free(entry_path);
        if (!is_regular_file) {
            continue;
        }
        if (count == INT32_MAX) {
            ok = false;
            break;
        }
        ++count;
    }

    const bool close_ok = closedir(directory) == 0;
    if (!ok || !close_ok) {
        return false;
    }
    *output = count;
    return true;
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

static char **aurex_std_host_c_build_argv(const uint8_t *program, const uint8_t **args, int32_t arg_count) {
    if (program == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) {
        return NULL;
    }

    char **argv = (char **)calloc((size_t)arg_count + 2, sizeof(char *));
    if (argv == NULL) {
        return NULL;
    }

    argv[0] = (char *)program;
    for (int32_t i = 0; i < arg_count; ++i) {
        argv[i + 1] = (char *)args[i];
    }
    argv[arg_count + 1] = NULL;
    return argv;
}

static int32_t aurex_std_host_c_decode_process_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 126;
}

static int32_t aurex_std_host_c_wait_for_child(pid_t child) {
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return 126;
    }
    return aurex_std_host_c_decode_process_status(status);
}

int32_t aurex_std_v0_run_process(const uint8_t *program, const uint8_t **args, int32_t arg_count) {
    if (program == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) {
        return 127;
    }

    char **argv = aurex_std_host_c_build_argv(program, args, arg_count);
    if (argv == NULL) {
        return 126;
    }

    const pid_t child = fork();
    if (child < 0) {
        free(argv);
        return 126;
    }

    if (child == 0) {
        execvp(argv[0], argv);
        _exit(errno == ENOENT ? 127 : 126);
    }

    const int32_t status = aurex_std_host_c_wait_for_child(child);
    free(argv);
    return status;
}

static bool aurex_std_host_c_append_capture(
    uint8_t **data,
    size_t *len,
    size_t *capacity,
    const uint8_t *chunk,
    size_t chunk_len
) {
    if (chunk_len == 0) {
        return true;
    }
    const size_t required = *len + chunk_len + 1;
    if (required < *len) {
        return false;
    }
    if (required > *capacity) {
        size_t next_capacity = *capacity == 0 ? 4096 : *capacity;
        while (next_capacity < required) {
            if (next_capacity > ((size_t)-1) / 2) {
                next_capacity = required;
                break;
            }
            next_capacity *= 2;
        }
        uint8_t *next_data = (uint8_t *)realloc(*data, next_capacity);
        if (next_data == NULL) {
            return false;
        }
        *data = next_data;
        *capacity = next_capacity;
    }
    for (size_t i = 0; i < chunk_len; ++i) {
        (*data)[*len + i] = chunk[i];
    }
    *len += chunk_len;
    (*data)[*len] = 0;
    return true;
}

static AurexStdProcessOutput aurex_std_host_c_process_output(
    int32_t status,
    uint8_t *stdout_data,
    int32_t stdout_len,
    uint8_t *stderr_data,
    int32_t stderr_len
) {
    AurexStdProcessOutput result;
    result.status = status;
    result.stdout_data = stdout_data;
    result.stdout_len = stdout_len;
    result.stderr_data = stderr_data;
    result.stderr_len = stderr_len;
    return result;
}

static void aurex_std_host_c_close_fd(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

bool aurex_std_v0_run_process_capture(
    const uint8_t *program,
    const uint8_t **args,
    int32_t arg_count,
    AurexStdProcessOutput *output
) {
    if (output == NULL) {
        return false;
    }
    *output = aurex_std_host_c_process_output(126, NULL, 0, NULL, 0);
    if (program == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) {
        *output = aurex_std_host_c_process_output(127, NULL, 0, NULL, 0);
        return true;
    }

    char **argv = aurex_std_host_c_build_argv(program, args, arg_count);
    if (argv == NULL) {
        return true;
    }

    int stdout_pipe[2];
    if (pipe(stdout_pipe) < 0) {
        free(argv);
        return true;
    }
    int stderr_pipe[2];
    if (pipe(stderr_pipe) < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        free(argv);
        return true;
    }

    const pid_t child = fork();
    if (child < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        free(argv);
        return true;
    }

    if (child == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        execvp(argv[0], argv);
        _exit(errno == ENOENT ? 127 : 126);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    int stdout_fd = stdout_pipe[0];
    int stderr_fd = stderr_pipe[0];
    uint8_t *stdout_data = NULL;
    size_t stdout_len = 0;
    size_t stdout_capacity = 0;
    uint8_t *stderr_data = NULL;
    size_t stderr_len = 0;
    size_t stderr_capacity = 0;
    uint8_t buffer[4096];
    bool capture_ok = true;
    int active = 2;
    while (active > 0) {
        struct pollfd fds[2];
        fds[0].fd = stdout_fd;
        fds[0].events = stdout_fd >= 0 ? POLLIN : 0;
        fds[0].revents = 0;
        fds[1].fd = stderr_fd;
        fds[1].events = stderr_fd >= 0 ? POLLIN : 0;
        fds[1].revents = 0;

        const int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            capture_ok = false;
            break;
        }

        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0 || fds[i].revents == 0) {
                continue;
            }
            uint8_t **data = i == 0 ? &stdout_data : &stderr_data;
            size_t *len = i == 0 ? &stdout_len : &stderr_len;
            size_t *capacity = i == 0 ? &stdout_capacity : &stderr_capacity;
            int *fd = i == 0 ? &stdout_fd : &stderr_fd;

            const ssize_t count = read(*fd, buffer, sizeof(buffer));
            if (count > 0) {
                if (!aurex_std_host_c_append_capture(data, len, capacity, buffer, (size_t)count)) {
                    capture_ok = false;
                    break;
                }
                continue;
            }
            if (count == 0) {
                aurex_std_host_c_close_fd(fd);
                --active;
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            capture_ok = false;
            break;
        }
        if (!capture_ok) {
            break;
        }
    }
    aurex_std_host_c_close_fd(&stdout_fd);
    aurex_std_host_c_close_fd(&stderr_fd);

    const int32_t status = aurex_std_host_c_wait_for_child(child);
    free(argv);
    if (!capture_ok || stdout_len > (size_t)INT32_MAX || stderr_len > (size_t)INT32_MAX) {
        free(stdout_data);
        free(stderr_data);
        return true;
    }

    *output = aurex_std_host_c_process_output(status, stdout_data, (int32_t)stdout_len, stderr_data, (int32_t)stderr_len);
    return true;
}

void aurex_std_v0_free_process_output_data(uint8_t *data) {
    free(data);
}
