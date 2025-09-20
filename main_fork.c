#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "huffman.h"
#include "file_buffer.h"
#include "mem_buffer.h"

#define MAX_FILES 1000
#define MAX_PROCESSES 8 // Número máximo de procesos paralelos

// Estructura para pasar información entre procesos padre e hijos
struct compress_task
{
    char input_path[512];
    char filename[256];
    size_t file_index;
    int pipe_fd; // Para comunicación padre-hijo
};

// Estructura para resultados de compresión
struct compress_result
{
    char filename[256];
    size_t compressed_size;
    size_t file_index;
    int success;
};

int file_encode(const char *input_file, const char *output_file)
{
    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in)
    {
        LOGE("Error: No se pudo crear el buffer de archivo de entrada\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out)
    {
        LOGE("Error: No se pudo crear el buffer de archivo de salida\n");
        goto OUT;
    }
    encode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

int file_decode(const char *input_file, const char *output_file)
{
    struct buffer_ops *in, *out;

    in = create_file_buffer_ops(input_file, "rb");
    if (!in)
    {
        LOGE("Error: No se pudo crear el buffer de archivo de entrada\n");
        return 1;
    }
    out = create_file_buffer_ops(output_file, "wb");
    if (!out)
    {
        LOGE("Error: No se pudo crear el buffer de archivo de salida\n");
        goto OUT;
    }
    decode(in, out);
    desotry_file_buffer_ops(out);
OUT:
    desotry_file_buffer_ops(in);
    return 0;
}

// Función que ejecuta cada proceso hijo para comprimir un archivo
void compress_file_task(struct compress_task *task)
{
    struct compress_result result = {0};
    strcpy(result.filename, task->filename);
    result.file_index = task->file_index;
    result.success = 0;

    // Leer tamaño del archivo de entrada
    FILE *fin = fopen(task->input_path, "rb");
    if (!fin)
    {
        result.success = 0;
        write(task->pipe_fd, &result, sizeof(result));
        close(task->pipe_fd);
        return;
    }

    fseek(fin, 0, SEEK_END);
    size_t in_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    fclose(fin);

    // Codificar a buffer en memoria
    size_t mem_buf_size = in_size * 2 + 1024;
    void *mem_buf = malloc(mem_buf_size);
    if (!mem_buf)
    {
        result.success = 0;
        write(task->pipe_fd, &result, sizeof(result));
        close(task->pipe_fd);
        return;
    }

    struct buffer_ops *in_ops = create_file_buffer_ops(task->input_path, "rb");
    struct buffer_ops *mem_ops = create_mem_buffer_ops(mem_buf, mem_buf_size);

    if (!in_ops || !mem_ops)
    {
        free(mem_buf);
        result.success = 0;
        write(task->pipe_fd, &result, sizeof(result));
        close(task->pipe_fd);
        return;
    }

    encode(in_ops, mem_ops);

    // Obtener tamaño codificado
    struct mem_region *pmr = (struct mem_region *)mem_ops->data;
    result.compressed_size = pmr->cur;

    // Crear archivo temporal para este proceso
    char temp_file[512]; // Buffer más grande
    snprintf(temp_file, sizeof(temp_file), "/tmp/huff_temp_%zu_%d", task->file_index, getpid());

    FILE *temp_fp = fopen(temp_file, "wb");
    if (temp_fp)
    {
        // Escribir metadata y datos comprimidos al archivo temporal
        uint8_t name_len = (uint8_t)strlen(task->filename);
        fwrite(&name_len, 1, 1, temp_fp);
        fwrite(task->filename, 1, name_len, temp_fp);
        fwrite(&result.compressed_size, sizeof(uint32_t), 1, temp_fp);
        fwrite(mem_buf, 1, result.compressed_size, temp_fp);
        fclose(temp_fp);

        result.success = 1;
    }
    else
    {
        result.success = 0;
    }

    desotry_file_buffer_ops(in_ops);
    desotry_mem_buffer_ops(mem_ops);
    free(mem_buf);

    // Enviar resultado al proceso padre
    write(task->pipe_fd, &result, sizeof(result));
    close(task->pipe_fd);
}

// Función para combinar archivos temporales en el archivo final
int combine_temp_files(const char *output_file, struct compress_result *results, int num_files)
{
    FILE *archive = fopen(output_file, "wb");
    if (!archive)
    {
        LOGE("Error: No se pudo crear el archivo de salida único.\n");
        return 1;
    }

    // Ordenar resultados por índice de archivo para mantener el orden
    for (int i = 0; i < num_files - 1; i++)
    {
        for (int j = i + 1; j < num_files; j++)
        {
            if (results[i].file_index > results[j].file_index)
            {
                struct compress_result temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Combinar archivos temporales
    for (int i = 0; i < num_files; i++)
    {
        if (!results[i].success)
            continue;

        char temp_file[512]; // Aumentar tamaño del buffer

        // Buscar el archivo temporal creado por cualquier proceso hijo
        DIR *temp_dir = opendir("/tmp");
        if (temp_dir)
        {
            struct dirent *entry;
            char pattern[64];
            snprintf(pattern, sizeof(pattern), "huff_temp_%zu_", results[i].file_index);

            // Inicializar temp_file con valor por defecto
            snprintf(temp_file, sizeof(temp_file), "/tmp/huff_temp_%zu_0", results[i].file_index);

            while ((entry = readdir(temp_dir)))
            {
                if (strncmp(entry->d_name, pattern, strlen(pattern)) == 0)
                {
                    snprintf(temp_file, sizeof(temp_file), "/tmp/%s", entry->d_name);
                    break;
                }
            }
            closedir(temp_dir);
        }
        else
        {
            // Si no se puede abrir /tmp, usar nombre por defecto
            snprintf(temp_file, sizeof(temp_file), "/tmp/huff_temp_%zu_0", results[i].file_index);
        }

        FILE *temp_fp = fopen(temp_file, "rb");
        if (temp_fp)
        {
            char buffer[8192];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), temp_fp)) > 0)
            {
                fwrite(buffer, 1, bytes_read, archive);
            }
            fclose(temp_fp);
            unlink(temp_file); // Eliminar archivo temporal
        }
    }

    fclose(archive);
    return 0;
}

// Función para descompresión paralela mejorada
int parallel_decompress(const char *archive_file)
{
    FILE *archive = fopen(archive_file, "rb");
    if (!archive)
    {
        LOGE("Error: No se pudo abrir el archivo único para descomprimir.\n");
        return 1;
    }

    // Estructura para información de archivos
    struct
    {
        char filename[256];
        uint32_t size;
        long offset;
    } file_info[MAX_FILES];

    int file_count = 0;

    // Primera pasada: recopilar información de todos los archivos (secuencial)
    while (file_count < MAX_FILES)
    {
        uint8_t name_len;
        if (fread(&name_len, 1, 1, archive) != 1)
            break;

        if (fread(file_info[file_count].filename, 1, name_len, archive) != name_len)
            break;
        file_info[file_count].filename[name_len] = '\0';

        if (fread(&file_info[file_count].size, sizeof(uint32_t), 1, archive) != 1)
            break;

        file_info[file_count].offset = ftell(archive);
        fseek(archive, file_info[file_count].size, SEEK_CUR);
        file_count++;
    }

    fclose(archive);

    if (file_count == 0)
    {
        LOGE("Error: No se encontraron archivos en el archivo comprimido.\n");
        return 1;
    }

    // Segunda fase: descompresión paralela
    int active_processes = 0;
    int files_processed = 0;
    pid_t child_pids[MAX_PROCESSES];

    while (files_processed < file_count)
    {
        // Lanzar nuevos procesos hasta el límite
        while (active_processes < MAX_PROCESSES && files_processed + active_processes < file_count)
        {
            int current_file = files_processed + active_processes;

            pid_t pid = fork();
            if (pid == 0)
            {
                // Proceso hijo: descomprimir un archivo específico
                FILE *archive_child = fopen(archive_file, "rb");
                if (!archive_child)
                    exit(1);

                // Ir a la posición del archivo en el archivo comprimido
                if (fseek(archive_child, file_info[current_file].offset, SEEK_SET) != 0)
                {
                    fclose(archive_child);
                    exit(1);
                }

                // Leer datos comprimidos
                char *buf = malloc(file_info[current_file].size);
                if (!buf)
                {
                    fclose(archive_child);
                    exit(1);
                }

                if (fread(buf, 1, file_info[current_file].size, archive_child) != file_info[current_file].size)
                {
                    free(buf);
                    fclose(archive_child);
                    exit(1);
                }

                fclose(archive_child);

                // Decodificar desde buffer en memoria
                struct buffer_ops *mem_ops = create_mem_buffer_ops(buf, file_info[current_file].size);
                char output_path[512];
                snprintf(output_path, sizeof(output_path), "librosHunzip/%s", file_info[current_file].filename);
                struct buffer_ops *out_ops = create_file_buffer_ops(output_path, "wb");

                if (mem_ops && out_ops)
                {
                    decode(mem_ops, out_ops);
                    desotry_mem_buffer_ops(mem_ops);
                    desotry_file_buffer_ops(out_ops);
                    free(buf);
                    exit(0); // Éxito
                }
                else
                {
                    if (mem_ops)
                        desotry_mem_buffer_ops(mem_ops);
                    if (out_ops)
                        desotry_file_buffer_ops(out_ops);
                    free(buf);
                    exit(1); // Error
                }
            }
            else if (pid > 0)
            {
                // Proceso padre: guardar PID del hijo
                child_pids[active_processes] = pid;
                active_processes++;
            }
            else
            {
                perror("fork en descompresión");
                return 1;
            }
        }

        // Esperar a que termine al menos un proceso
        if (active_processes > 0)
        {
            int status;
            pid_t finished_pid = wait(&status);

            // Verificar si el proceso terminó correctamente
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                LOGE("Error: Proceso de descompresión falló\n");
            }

            // Buscar y remover el PID terminado
            for (int i = 0; i < active_processes; i++)
            {
                if (child_pids[i] == finished_pid)
                {
                    // Mover el último elemento a esta posición
                    for (int j = i; j < active_processes - 1; j++)
                    {
                        child_pids[j] = child_pids[j + 1];
                    }
                    break;
                }
            }
            active_processes--;
            files_processed++;
        }
    }

    // Esperar a que terminen todos los procesos restantes
    while (active_processes > 0)
    {
        int status;
        wait(&status);
        active_processes--;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Opción -empty
    if (argc >= 2 && !strcmp(argv[1], "-empty"))
    {
        struct dirent *entry;
        DIR *dp;
        char file_path[1024];

        // Empty librosHzip
        dp = opendir("librosHzip");
        if (dp != NULL)
        {
            while ((entry = readdir(dp)))
            {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                    continue;
                snprintf(file_path, sizeof(file_path), "librosHzip/%s", entry->d_name);
                struct stat st;
                if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode))
                {
                    char cmd[1100];
                    snprintf(cmd, sizeof(cmd), "gio trash '%s'", file_path);
                    system(cmd);
                }
            }
            closedir(dp);
        }

        // Empty librosHunzip
        dp = opendir("librosHunzip");
        if (dp != NULL)
        {
            while ((entry = readdir(dp)))
            {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                    continue;
                snprintf(file_path, sizeof(file_path), "librosHunzip/%s", entry->d_name);
                struct stat st;
                if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode))
                {
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

    if (argc < 2)
    {
        // COMPRESIÓN PARALELA
        clock_t start_time = clock();

        DIR *dp = opendir("libros");
        if (dp == NULL)
        {
            LOGE("Error: No se pudo abrir el directorio 'libros'.\n");
            return 1;
        }

        // Recopilar todos los archivos .txt
        struct compress_task tasks[MAX_FILES];
        int num_files = 0;
        struct dirent *entry;

        while ((entry = readdir(dp)) && num_files < MAX_FILES)
        {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, ".txt"))
            {
                snprintf(tasks[num_files].input_path, sizeof(tasks[num_files].input_path),
                         "libros/%s", entry->d_name);
                strcpy(tasks[num_files].filename, entry->d_name);
                tasks[num_files].file_index = num_files;
                num_files++;
            }
        }
        closedir(dp);

        if (num_files == 0)
        {
            LOGI("No se encontraron archivos .txt para comprimir.\n");
            return 0;
        }

        // Comprimiendo archivos en paralelo

        // Crear pipes para comunicación
        int pipes[MAX_FILES][2];
        pid_t child_pids[MAX_FILES];
        struct compress_result results[MAX_FILES];

        int active_processes = 0;
        int files_processed = 0;

        // Lanzar procesos de compresión
        while (files_processed < num_files)
        {
            // Lanzar nuevos procesos hasta el límite
            while (active_processes < MAX_PROCESSES && files_processed + active_processes < num_files)
            {
                int current_task = files_processed + active_processes;

                if (pipe(pipes[current_task]) == -1)
                {
                    perror("pipe");
                    return 1;
                }

                tasks[current_task].pipe_fd = pipes[current_task][1];

                pid_t pid = fork();
                if (pid == 0)
                {
                    // Proceso hijo
                    close(pipes[current_task][0]); // Cerrar extremo de lectura
                    compress_file_task(&tasks[current_task]);
                    exit(0);
                }
                else if (pid > 0)
                {
                    // Proceso padre
                    child_pids[active_processes] = pid;
                    close(pipes[current_task][1]); // Cerrar extremo de escritura
                    active_processes++;
                }
                else
                {
                    perror("fork");
                    return 1;
                }
            }

            // Esperar a que termine al menos un proceso
            if (active_processes > 0)
            {
                int status;
                pid_t finished_pid = wait(&status);

                // Buscar el proceso terminado y leer su resultado
                for (int i = 0; i < active_processes; i++)
                {
                    if (child_pids[i] == finished_pid)
                    {
                        int task_index = files_processed + i;
                        read(pipes[task_index][0], &results[task_index], sizeof(struct compress_result));
                        close(pipes[task_index][0]);

                        // Reorganizar arrays
                        for (int j = i; j < active_processes - 1; j++)
                        {
                            child_pids[j] = child_pids[j + 1];
                        }
                        break;
                    }
                }
                active_processes--;
                files_processed++;
            }
        }

        // Esperar procesos restantes
        while (active_processes > 0)
        {
            int status;
            pid_t finished_pid = wait(&status);

            for (int i = 0; i < active_processes; i++)
            {
                if (child_pids[i] == finished_pid)
                {
                    int task_index = files_processed + i;
                    read(pipes[task_index][0], &results[task_index], sizeof(struct compress_result));
                    close(pipes[task_index][0]);
                    break;
                }
            }
            active_processes--;
            files_processed++;
        }

        // Combinar resultados
        combine_temp_files("librosHzip/archivolibros.huff", results, num_files);

        clock_t end_time = clock();
        double elapsed_ms = ((double)(end_time - start_time)) * 1000.0 / CLOCKS_PER_SEC;
        printf("Tiempo de compresión: %.2f ms\n", elapsed_ms);

        // DESCOMPRESIÓN PARALELA
        clock_t start_dec_time = clock();
        parallel_decompress("librosHzip/archivolibros.huff");

        clock_t end_dec_time = clock();
        double elapsed_dec_ms = ((double)(end_dec_time - start_dec_time)) * 1000.0 / CLOCKS_PER_SEC;
        printf("Tiempo de descompresión: %.2f ms\n", elapsed_dec_ms);

        return 0;
    }

    // Opciones de línea de comandos
    if (argc < 3)
    {
        LOGE("Error: Faltan argumentos.\n");
        LOGE("Uso: -d/-e entrada [salida]\n");
        return 1;
    }

    char *input = argv[2];
    char *output = NULL;
    if (argc > 3)
    {
        output = argv[3];
    }

    if (!strcmp(argv[1], "-e"))
    {
        file_encode(input, output);
    }
    else if (!strcmp(argv[1], "-d"))
    {
        file_decode(input, output);
    }
    else
    {
        LOGE("Error: Acción desconocida.\n");
    }

    return 0;
}
