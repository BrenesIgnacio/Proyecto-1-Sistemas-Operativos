#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "huffman.h"
#include "file_buffer.h"
#include "mem_buffer.h"

// Programa principal para comprimir y descomprimir archivos usando Huffman.
// Codifica todos los .txt de 'libros/' en un solo archivo comprimido y luego los descomprime.

/*
TODO:
- Pthreads para comprimir y descomprimir en paralelo.
- Fork para comprimir y descomprimir en paralelo.
- Agregar el resto de libros en el folder libros.
*/

int file_encode(const char *input_file, const char *output_file) {
    // Codifica un archivo de entrada a un archivo de salida usando buffer_ops

    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in) {
        LOGE("Error: No se pudo crear el buffer de archivo de entrada\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out) {
        LOGE("Error: No se pudo crear el buffer de archivo de salida\n");
        goto OUT;
    }
    encode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

int file_decode(const char *input_file, const char *output_file) {
    // Decodifica un archivo comprimido a un archivo de salida usando buffer_ops

    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in) {
        LOGE("Error: No se pudo crear el buffer de archivo de entrada\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out) {
        LOGE("Error: No se pudo crear el buffer de archivo de salida\n");
        goto OUT;
    }
    decode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

int main(int argc, char *argv[]) {
    // Si se pasa el argumento -empty, vacía los directorios de salida
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
    LOGI("librosHzip y librosHunzip vaciados a la papelera.\n");
        return 0;
    }
    if (argc < 2) {

        // Si no hay argumentos, se comprimen todos los .txt en 'libros/' a un solo archivo en 'librosHzip/'
        // y luego se descomprime ese archivo en 'librosHunzip/'.
        // Medición de tiempo de compresión
        clock_t start_time = clock();

        // Crear archivo de salida único para todos los libros comprimidos
        FILE *archive = fopen("librosHzip/archivolibros.huff", "wb");
        if (!archive) {
            LOGE("Error: No se pudo crear el archivo de salida único.\n");
            return 1;
        }

        // Declarar variables para directorio y entradas
        DIR *dp;
        struct dirent *entry;

        // Abrir directorio de libros de entrada
        dp = opendir("libros");
        if (dp == NULL) {
            LOGE("Error: No se pudo abrir el directorio 'libros'.\n");
            fclose(archive);
            return 1;
        }
        // Recorrer todos los archivos .txt y comprimir
        while ((entry = readdir(dp))) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, ".txt")) {
                char input_path[512];
                snprintf(input_path, sizeof(input_path), "libros/%s", entry->d_name);

                // Leer tamaño del archivo de entrada
                FILE *fin = fopen(input_path, "rb");
                if (!fin) continue;
                fseek(fin, 0, SEEK_END);
                size_t in_size = ftell(fin);
                fseek(fin, 0, SEEK_SET);
                fclose(fin);

                // Codificar a buffer en memoria
                size_t mem_buf_size = in_size * 2 + 1024;
                void *mem_buf = malloc(mem_buf_size);
                struct buffer_ops *in_ops = create_file_buffer_ops(input_path, "rb");
                struct buffer_ops *mem_ops = create_mem_buffer_ops(mem_buf, mem_buf_size);
                encode(in_ops, mem_ops);

                // Obtener tamaño codificado
                struct mem_region *pmr = (struct mem_region *)mem_ops->data;
                uint32_t encoded_size = pmr->cur;

                // Escribir metadata (nombre, tamaño) y datos al archivo único
                uint8_t name_len = (uint8_t)strlen(entry->d_name);
                fwrite(&name_len, 1, 1, archive);
                fwrite(entry->d_name, 1, name_len, archive);
                fwrite(&encoded_size, sizeof(uint32_t), 1, archive);
                fwrite(mem_buf, 1, encoded_size, archive);

                desotry_file_buffer_ops(in_ops);
                desotry_mem_buffer_ops(mem_ops);
                free(mem_buf);
            }
        }
        closedir(dp);
        fclose(archive);

        // Fin de compresión y medición de tiempo
        clock_t end_time = clock();
        double elapsed_ms = ((double)(end_time - start_time)) * 1000.0 / CLOCKS_PER_SEC;
        printf("Tiempo de compresión: %.2f ms\n", elapsed_ms);

        // Medición de tiempo de descompresión
        clock_t start_dec_time = clock();

        // Abrir archivo único para leer y descomprimir
        archive = fopen("librosHzip/archivolibros.huff", "rb");
        if (!archive) {
            LOGE("Error: No se pudo abrir el archivo único para descomprimir.\n");
            return 1;
        }
        // Leer y extraer cada archivo del archivo único
        while (1) {
            uint8_t name_len;
            // Leer longitud del nombre del archivo
            if (fread(&name_len, 1, 1, archive) != 1) break; // EOF
            char filename[256] = {0};
            // Leer nombre del archivo
            if (fread(filename, 1, name_len, archive) != name_len) break;
            uint32_t size;
            // Leer tamaño de los datos comprimidos
            if (fread(&size, sizeof(uint32_t), 1, archive) != 1) break;
            char *buf = malloc(size);
            // Leer datos comprimidos
            if (fread(buf, 1, size, archive) != size) { free(buf); break; }

            // Decodificar desde buffer en memoria y escribir archivo de salida
            struct buffer_ops *mem_ops = create_mem_buffer_ops(buf, size);
            char output_path[512];
            snprintf(output_path, sizeof(output_path), "librosHunzip/%s", filename);
            struct buffer_ops *out_ops = create_file_buffer_ops(output_path, "wb");
            decode(mem_ops, out_ops);
            desotry_mem_buffer_ops(mem_ops);
            desotry_file_buffer_ops(out_ops);
            free(buf);
        }
        fclose(archive);

        clock_t end_dec_time = clock();
        double elapsed_dec_ms = ((double)(end_dec_time - start_dec_time)) * 1000.0 / CLOCKS_PER_SEC;
    printf("Tiempo de descompresión: %.2f ms\n", elapsed_dec_ms);
        return 0;
    }
    if (argc < 3) {
    LOGE("Error: Faltan argumentos.\n");
    LOGE("Uso: -d/-e entrada [salida]\n");
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
    LOGE("Error: Acción desconocida.\n");
    }
    return 0;
}
