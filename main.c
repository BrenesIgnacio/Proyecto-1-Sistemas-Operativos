#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include "huffman.h"
#include "simple_file_buffer.h"

/*
TODO:
- Hay que cambiar un poco la logica del main para que en lugar de crear varios archivos comprimidos
genere un solo archivo comprimido que contenga todos los libros.
- Hay que agregar el tiempo de compresion y descompresion en el output del programa. (milisegundos)
- Pthreads para comprimir y descomprimir en paralelo.
- Fork para comprimir y descomprimir en paralelo.
- Agregar el resto de libros en el folder libros.
- traducir prints a espanol.
*/

int file_encode(const char *input_file, const char *output_file) {

    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in) {
        LOGE("Create input file ops failed\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out) {
        LOGE("Create input file ops failed\n");
        goto OUT;
    }
    encode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

int file_decode(const char *input_file, const char *output_file) {

    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in) {
        LOGE("Create input file ops failed\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out) {
        LOGE("Create input file ops failed\n");
        goto OUT;
    }
    decode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

int main(int argc, char *argv[]) {
    char *input, *output;
    // -empty
    if (argc >= 2 && !strcmp(argv[1], "-empty")) {
        struct dirent *entry;
        DIR *dp;
        char file_path[1024];
        // Empty librosHzip
        dp = opendir("librosHzip");
        if (dp != NULL) {
            while ((entry = readdir(dp))) {
                // Skip . and ..
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
                snprintf(file_path, sizeof(file_path), "librosHzip/%s", entry->d_name);
                struct stat st;
                if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    char cmd[1100];
                    snprintf(cmd, sizeof(cmd), "gio trash '%s'", file_path);
                    system(cmd);
                }
            }
            closedir(dp);
        }
        // Empty librosHunzip
        dp = opendir("librosHunzip");
        if (dp != NULL) {
            while ((entry = readdir(dp))) {
                // Skip . and ..
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
                snprintf(file_path, sizeof(file_path), "librosHunzip/%s", entry->d_name);
                struct stat st;
                if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    char cmd[1100];
                    snprintf(cmd, sizeof(cmd), "gio trash '%s'", file_path);
                    system(cmd);
                }
            }
            closedir(dp);
        }
        LOGI("Emptied librosHzip and librosHunzip to trash.\n");
        return 0;
    }
    if (argc < 2) {
        // Si no hay argumentos: se comprimen todos los.txt en libros a librosHzip, luego descomprime todos 
        //los .huff en librosHzip a librosHunzip
        #include <dirent.h>
        #include <stdio.h>
        struct dirent *entry;
        DIR *dp;
        // Se comprimen los .txt en libros a librosHzip
        dp = opendir("libros");
        if (dp == NULL) {
            LOGE("Cannot open libros directory.\n");
            return 1;
        }
        while ((entry = readdir(dp))) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, ".txt")) {
                char input_path[512], output_path[512];
                snprintf(input_path, sizeof(input_path), "libros/%s", entry->d_name);
                snprintf(output_path, sizeof(output_path), "librosHzip/%s.huff", entry->d_name);
                LOGI("Encoding %s -> %s\n", input_path, output_path);
                file_encode(input_path, output_path);
            }
        }
        closedir(dp);

        // Descomprimen los .huff en librosHzip a librosHunzip
        dp = opendir("librosHzip");
        if (dp == NULL) {
            LOGE("Cannot open librosHzip directory.\n");
            return 1;
        }
        while ((entry = readdir(dp))) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, ".huff")) {
                char input_path[512], output_path[512];
                snprintf(input_path, sizeof(input_path), "librosHzip/%s", entry->d_name);
                char base_name[499];
                strncpy(base_name, entry->d_name, sizeof(base_name) - 1);
                base_name[sizeof(base_name) - 1] = '\0';
                char *dot = strrchr(base_name, '.');
                if (dot) *dot = '\0';
                snprintf(output_path, sizeof(output_path), "librosHunzip/%s", base_name);
                LOGI("Decoding %s -> %s\n", input_path, output_path);
                file_decode(input_path, output_path);
            }
        }
        closedir(dp);
        return 0;
    }
    if (argc < 3) {
        LOGE("Miss args.\n");
        LOGE("Usage: -d/-e input [output]\n");
        return 1;
    }
    input = argv[2];
    output = NULL;
    if (argc > 3) {
        output = argv[3];
    }
    if (!strcmp(argv[1], "-e")) {
        file_encode(input, output);
    } else if (!strcmp(argv[1], "-d")) {
        file_decode(input, output);
    } else {
        LOGE("Unknown action.\n");
    }
    return 0;
}
