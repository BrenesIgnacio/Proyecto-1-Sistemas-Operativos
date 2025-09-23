#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "huffman.h"
#include "file_buffer.h"
#include "mem_buffer.h"

// Estructura para compresión
typedef struct {
    char input_path[512];
    char filename[256];
    size_t in_size;
} compress_job_t;

// Estructura para descompresión
typedef struct {
    char filename[256];
    uint32_t size;
    char output_path[512];
    void *buf;
} decompress_job_t;


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

int compress_worker(void *arg) {
    // Toma la lista de trabajos (archivo y su información) y los procesa secuencialmente
    compress_job_t *jobs = ((compress_job_t **)arg)[0];
    int job_count = *((int *)((compress_job_t **)arg)[1]);
    FILE *archive = ((FILE **)((compress_job_t **)arg)[2])[0];

    for (int i = 0; i < job_count; ++i) {
        compress_job_t *job = &jobs[i];

        // Codificar a buffer en memoria
        size_t mem_buf_size = job->in_size * 2 + 1024;
        void *mem_buf = malloc(mem_buf_size);
        struct buffer_ops *in_ops = create_file_buffer_ops(job->input_path, "rb");
        struct buffer_ops *mem_ops = create_mem_buffer_ops(mem_buf, mem_buf_size);
        encode(in_ops, mem_ops);
        struct mem_region *pmr = (struct mem_region *)mem_ops->data;
        uint32_t encoded_size = pmr->cur;

        // Escribir metadata y datos al archivo único
        uint8_t name_len = (uint8_t)strlen(job->filename);
        fwrite(&name_len, 1, 1, archive);
        fwrite(job->filename, 1, name_len, archive);
        fwrite(&encoded_size, sizeof(uint32_t), 1, archive);
        fwrite(mem_buf, 1, encoded_size, archive);
        desotry_file_buffer_ops(in_ops);
        desotry_mem_buffer_ops(mem_ops);
        free(mem_buf);
    }
    return 0;
}

int decompress_worker(void *arg) {
    decompress_job_t *jobs = ((decompress_job_t **)arg)[0];
    int job_count = *((int *)((decompress_job_t **)arg)[1]);
    
    for (int i = 0; i < job_count; ++i) {
        decompress_job_t *job = &jobs[i];
        struct buffer_ops *mem_ops = create_mem_buffer_ops(job->buf, job->size);
        struct buffer_ops *out_ops = create_file_buffer_ops(job->output_path, "wb");
        decode(mem_ops, out_ops);
        desotry_mem_buffer_ops(mem_ops);
        desotry_file_buffer_ops(out_ops);
        free(job->buf);
    }
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
        // COMPRESIÓN CONCURRENTE (1 hilo worker)
        clock_t start_time = clock();

        // Preparar trabajos de compresión
        DIR *dp = opendir("libros");
        if (dp == NULL) {
            LOGE("Error: No se pudo abrir el directorio 'libros'.\n");
            return 1;
        }
        compress_job_t jobs[256];
        int job_count = 0;
        struct dirent *entry;
        while ((entry = readdir(dp))) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, ".txt")) {
                snprintf(jobs[job_count].input_path, sizeof(jobs[job_count].input_path), "libros/%s", entry->d_name);
                strncpy(jobs[job_count].filename, entry->d_name, sizeof(jobs[job_count].filename));
                FILE *fin = fopen(jobs[job_count].input_path, "rb");
                if (!fin) continue;
                fseek(fin, 0, SEEK_END);
                jobs[job_count].in_size = ftell(fin);
                fclose(fin);
                job_count++;
            }
        }
        closedir(dp);

        FILE *archive = fopen("librosHzip/archivolibros.huff", "wb");
        if (!archive) {
            LOGE("Error: No se pudo crear el archivo de salida único.\n");
            return 1;
        }

        // Lanzar hilo worker para compresión
        pthread_t worker;
        void *args[3] = {jobs, &job_count, &archive};
        pthread_create(&worker, NULL, (void *(*)(void *))compress_worker, args);
        pthread_join(worker, NULL);
        fclose(archive);

        clock_t end_time = clock();
        double elapsed_ms = ((double)(end_time - start_time)) * 1000.0 / CLOCKS_PER_SEC;
        printf("Tiempo de compresión: %.2f ms\n", elapsed_ms);

        // DESCOMPRESIÓN CONCURRENTE (1 hilo worker)
        clock_t start_dec_time = clock();

        archive = fopen("librosHzip/archivolibros.huff", "rb");
        if (!archive) {
            LOGE("Error: No se pudo abrir el archivo único para descomprimir.\n");
            return 1;
        }
        decompress_job_t djobs[256];
        int djob_count = 0;
        while (1) {
            uint8_t name_len;
            if (fread(&name_len, 1, 1, archive) != 1) break;
            if (name_len > 250) break;
            if (fread(djobs[djob_count].filename, 1, name_len, archive) != name_len) break;
            djobs[djob_count].filename[name_len] = '\0';
            uint32_t size;
            if (fread(&size, sizeof(uint32_t), 1, archive) != 1) break;
            djobs[djob_count].size = size;
            djobs[djob_count].buf = malloc(size);
            if (fread(djobs[djob_count].buf, 1, size, archive) != size) { free(djobs[djob_count].buf); break; }
            char tempname[256];
            memcpy(tempname, djobs[djob_count].filename, name_len);
            tempname[name_len] = '\0';
            snprintf(djobs[djob_count].output_path, sizeof(djobs[djob_count].output_path), "librosHunzip/%s", tempname);
            djob_count++;
        }
        fclose(archive);

        pthread_t dworker;
        void *dargs[2] = {djobs, &djob_count};
        pthread_create(&dworker, NULL, (void *(*)(void *))decompress_worker, dargs);
        pthread_join(dworker, NULL);

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
