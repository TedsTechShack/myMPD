/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2019 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "../dist/src/sds/sds.h"
#include "sds_extras.h"
#include "log.h"
#include "utility.h"

sds jsonrpc_start_notify(sds buffer, const char *method) {
    buffer = sdscrop(buffer);
    buffer = sdscatfmt(buffer, "{\"jsonrpc\":\"2.0\",\"method\":");
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    buffer = sdscat(buffer, ",\"params\":{");
    return buffer;
}

sds jsonrpc_end_notify(sds buffer) {
    buffer = sdscat(buffer, "}}");
    return buffer;
}

sds jsonrpc_notify(sds buffer, const char *method) {
    buffer = sdscrop(buffer);
    buffer = sdscatfmt(buffer, "{\"jsonrpc\":\"2.0\",\"method\":");
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    buffer = sdscat(buffer, ",\"params\":{}}");
    return buffer;
}

sds jsonrpc_start_result(sds buffer, const char *method, int id) {
    buffer = sdscrop(buffer);
    buffer = sdscatprintf(buffer, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"method\":", id);
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    return buffer;
}

sds jsonrpc_end_result(sds buffer) {
    buffer = sdscatfmt(buffer, "}}");
    return buffer;
}

sds jsonrpc_respond_ok(sds buffer, const char *method, int id) {
    buffer = sdscrop(buffer);
    buffer = sdscatprintf(buffer, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"method\":", id);
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    buffer = sdscat(buffer, ",\"message\":\"ok\"}}");
    return buffer;
}

sds jsonrpc_respond_message(sds buffer, const char *method, int id, const char *message, bool error) {
    buffer = sdscrop(buffer);
    buffer = sdscatprintf(buffer, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"%s\":{\"method\":", 
        id, (error == true ? "error" : "result"));
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    if (error == true) {
        buffer = sdscat(buffer, ",\"code\": -32000");
    }
    buffer = sdscat(buffer, ",\"message\":");
    buffer = sdscatjson(buffer, message, strlen(message)); /* Flawfinder: ignore */
    buffer = sdscatfmt(buffer, "}}");
    return buffer;
}

sds jsonrpc_start_phrase(sds buffer, const char *method, int id, const char *message, bool error) {
    buffer = sdscrop(buffer);
    buffer = sdscatprintf(buffer, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"%s\":{\"method\":", 
        id, (error == true ? "error" : "result"));
    buffer = sdscatjson(buffer, method, strlen(method)); /* Flawfinder: ignore */
    if (error == true) {
        buffer = sdscat(buffer, ",\"code\": -32000");
    }
    buffer = sdscat(buffer, ",\"message\":");
    buffer = sdscatjson(buffer, message, strlen(message)); /* Flawfinder: ignore */
    buffer = sdscat(buffer, ",\"data\":{");
    return buffer;
}

sds jsonrpc_end_phrase(sds buffer) {
    buffer = sdscat(buffer, "}}}");
    return buffer;
}

sds jsonrpc_start_phrase_notify(sds buffer, const char *message, bool error) {
    buffer = sdscrop(buffer);
    buffer = sdscatfmt(buffer, "{\"jsonrpc\":\"2.0\",\"%s\":{", 
        (error == true ? "error" : "result"));
    if (error == true) {
        buffer = sdscat(buffer, "\"code\": -32000,");
    }
    buffer = sdscat(buffer, "\"message\":");
    buffer = sdscatjson(buffer, message, strlen(message)); /* Flawfinder: ignore */
    buffer = sdscat(buffer, ",\"data\":{");
    return buffer;
}

sds tojson_char(sds buffer, const char *key, const char *value, bool comma) {
    buffer = sdscatfmt(buffer, "\"%s\":", key);
    if (value != NULL) {
        buffer = sdscatjson(buffer, value, strlen(value)); /* Flawfinder: ignore */
    }
    else {
        buffer = sdscat(buffer, "\"\"");
    }
    if (comma) {
        buffer = sdscat(buffer, ",");
    }
    return buffer;
}

sds tojson_char_len(sds buffer, const char *key, const char *value, size_t len, bool comma) {
    buffer = sdscatfmt(buffer, "\"%s\":", key);
    if (value != NULL) {
        buffer = sdscatjson(buffer, value, len);
    }
    else {
        buffer = sdscat(buffer, "\"\"");
    }
    if (comma) {
        buffer = sdscat(buffer, ",");
    }
    return buffer;
}

sds tojson_bool(sds buffer, const char *key, bool value, bool comma) {
    buffer = sdscatfmt(buffer, "\"%s\":%s", key, value == true ? "true" : "false");
    if (comma) {
        buffer = sdscat(buffer, ",");
    }
    return buffer;
}

sds tojson_long(sds buffer, const char *key, long value, bool comma) {
    buffer = sdscatprintf(buffer, "\"%s\":%ld", key, value);
    if (comma) {
        buffer = sdscat(buffer, ",");
    }
    return buffer;
}

sds tojson_float(sds buffer, const char *key, float value, bool comma) {
    buffer = sdscatprintf(buffer, "\"%s\":%f", key, value);
    if (comma) {
        buffer = sdscat(buffer, ",");
    }
    return buffer;
}

int testdir(const char *name, const char *dirname, bool create) {
    DIR* dir = opendir(dirname);
    if (dir) {
        closedir(dir);
        LOG_INFO("%s: \"%s\"", name, dirname);
        //directory exists
        return 0;
    }
    else {
        if (create == true) {
            if (mkdir(dirname, 0700) != 0) {
                LOG_ERROR("%s: creating \"%s\" failed", name, dirname);
                //directory not exists and creating it failed
                return 2;
            }
            else {
                LOG_INFO("%s: \"%s\" created", name, dirname);
                //directory successfully created
                return 1;
            }
        }
        else {
            LOG_ERROR("%s: \"%s\" don't exists", name, dirname);
            //directory not exists
            return 3;
        }
    }
}

int randrange(int n) {
    return rand() / (RAND_MAX / (n + 1) + 1);
}

bool validate_string(const char *data) {
    if (strchr(data, '/') != NULL || strchr(data, '\n') != NULL || strchr(data, '\r') != NULL ||
        strchr(data, '"') != NULL || strchr(data, '\'') != NULL || strchr(data, '\\') != NULL) {
        return false;
    }
    return true;
}

bool validate_uri(const char *data) {
    if (strstr(data, "/../") != NULL) {
        return false;
    }
    return true;
}

int replacechar(char *str, const char orig, const char rep) {
    char *ix = str;
    int n = 0;
    while ((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}

int uri_to_filename(char *str) {
    int n = replacechar(str, '/', '_');
    n+= replacechar(str, '.', '_');
    n+= replacechar(str, ':', '_');
    return n;
}

const struct mime_type_entry image_files[] = {
    {"png",  "image/png"},
    {"jpg",  "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"svg",  "image/svg+xml"},
    {"tiff", "image/tiff"},
    {"bmp",  "image/x-ms-bmp"},
    {NULL,   "application/octet-stream"}
};

sds find_image_file(sds basefilename) {
    const struct mime_type_entry *p = NULL;
    for (p = image_files; p->extension != NULL; p++) {
        sds testfilename = sdscatfmt(sdsempty(), "%s.%s", basefilename, p->extension);
        if (access(testfilename, F_OK) == 0) { /* Flawfinder: ignore */
            sdsfree(testfilename);
            break;
        }
        sdsfree(testfilename);
    }
    if (p->extension != NULL) {
        basefilename = sdscatfmt(basefilename, ".%s", p->extension);
    }
    else {
        basefilename = sdscrop(basefilename);
    }
    return basefilename;
}

const struct mime_type_entry media_files[] = {
    {"mp3",  "audio/mpeg"},
    {NULL,   "application/octet-stream"}
};

sds get_mime_type_by_ext(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return sdsempty();
    }
    else if (strlen(ext) > 1) {
        //trim starting dot
        ext++;
    }
    else {
        return sdsempty();        
    }

    const struct mime_type_entry *p = NULL;
    for (p = image_files; p->extension != NULL; p++) {
        if (strcmp(ext, p->extension) == 0) {
            break;
        }
    }
    if (p->extension == NULL) {
        p = NULL;
        for (p = media_files; p->extension != NULL; p++) {
            if (strcmp(ext, p->extension) == 0) {
                break;
            }
        }
    }
    sds mime_type = sdsnew(p->mime_type);
    return mime_type;
}

sds get_ext_by_mime_type(const char *mime_type) {
    const struct mime_type_entry *p = NULL;
    for (p = image_files; p->extension != NULL; p++) {
        if (strcmp(mime_type, p->mime_type) == 0) {
            break;
        }
    }
    sds ext = sdsnew(p->extension);
    return ext;
}

const struct magic_byte_entry magic_bytes[] = {
    {"89504E470D0A1A0A",  "image/png"},
    {"FFD8FFDB",  "image/jpeg"},
    {"FFD8FFE0",  "image/jpeg"},
    {"FFD8FFEE", "image/jpeg"},
    {"FFD8FFE1", "image/jpeg"},
    {"49492A00", "image/tiff"},
    {"4D4D002A", "image/tiff"},
    {"424D",  "image/x-ms-bmp"},
    {NULL,   "application/octet-stream"}
};

sds get_mime_type_by_magic(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        LOG_ERROR("Can't open %s", filename);
        return sdsempty();
    }
    unsigned char binary_buffer[8];
    size_t read = fread(binary_buffer, sizeof(binary_buffer), 1, fp);
    fclose(fp);
    sds stream = sdsnewlen(binary_buffer, read);
    sds mime_type = get_mime_type_by_magic_stream(stream);
    sdsfree(stream);
    return mime_type;
}

sds get_mime_type_by_magic_stream(sds stream) {
    sds hex_buffer = sdsempty();
    for (int i = 0; i < 8; i++) {
        hex_buffer = sdscatprintf(hex_buffer, "%02X", stream[i]);
    }
    LOG_DEBUG("First bytes in file: %s", hex_buffer);
    const struct magic_byte_entry *p = NULL;
    for (p = magic_bytes; p->magic_bytes != NULL; p++) {
        if (strncmp(hex_buffer, p->magic_bytes, strlen(p->magic_bytes)) == 0) {
            LOG_DEBUG("Matched magic bytes for mime_type: %s", p->mime_type);
            break;
        }
    }
    sdsfree(hex_buffer);
    sds mime_type = sdsnew(p->mime_type);
    return mime_type;
}
