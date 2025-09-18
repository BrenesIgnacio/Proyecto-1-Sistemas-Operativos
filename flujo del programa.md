## Descripción 

El sistema comprime varios archivos de texto (.txt) que están en `libros/` en un único archivo comprimido, y después los descomprine en `librosHunzip/`.

La implementación está estructurada en varios componentes:

1. **Sistema de codificación Huffman**: Implementado en `huffman.c` y `huffman.h`.
2. **Operaciones de buffer de archivos**: Implementado en `file_buffer.c` y `file_buffer.h`.
3. **Operaciones de buffer de memoria**: Implementado en `mem_buffer.c` y `mem_buffer.h`.
4. **Programa principal**: Implementado en `main.c`.

## Estructura del Código

### 1. Interfaz de Buffer (buffer_ops)

Es una abstracción común para operaciones de entrada/salida llamada `buffer_ops`, definida en `huffman.h`:

```c
struct buffer_ops {
    void *data;
    int (*eof)(struct buffer_ops *handle);
    int (*read)(struct buffer_ops *handle, void *buffer, size_t);
    int (*write)(struct buffer_ops *handle, void *data, size_t);
    int (*rewind)(struct buffer_ops *handle);
    void *priv_data;
};
```

Básicamente es una estructura unificada para trabajar con diferentes tipos de buffers (archivos o memoria), que permite que las funciones de compresión y descompresión funcionen de manera independiente a la fuente o destino de los datos.

### 2. Algoritmo de Huffman

#### Estructuras de Datos

- `struct huffman_node`: Es un nodo en el árbol Huffman, definido en `huffman.h`:
  ```c
  struct huffman_node {
      uint64_t weight;
      uint8_t value;
      struct huffman_node *left;
      struct huffman_node *right;
      struct huffman_node *__free_handle;
  };
  ```

- `struct huffman_code`: Almacena los códigos Huffman generados para cada símbolo:
  ```c
  struct huffman_code {
      uint8_t code[TABLE_SIZE];
      uint8_t length;
  };
  ```

- `struct huffman_file_header`: Es el encabezado que diferencia los archivos comprimidos:
  ```c
  struct huffman_file_header {
      char magic[8];
      uint64_t file_size;
      uint32_t table_size;
  };
  ```

#### Funciones Principales del Algoritmo

1. **build_huffman_tree** (líneas 73-115 en `huffman.c`): 
   - Construye el árbol Huffman a partir de una tabla de frecuencias.
   - Utiliza un algoritmo de ordenación para crear el árbol.
   - Devuelve el nodo raíz del árbol Huffman.

2. **generate_huffman_code** (líneas 53-56 en `huffman.c`):
   - Genera los códigos Huffman para cada símbolo a partir del árbol.
   - Utiliza `generate_huffman_code_recusive` (líneas 43-51) para recorrer el árbol recursivamente.

3. **encode** (líneas 123-198 en `huffman.c`):
   - Lee el archivo de entrada para calcular la frecuencia de cada byte.
   - Construye el árbol Huffman.
   - Genera los códigos para cada símbolo.
   - Escribe el encabezado y la tabla de frecuencias en el archivo de salida.
   - Codifica los datos utilizando los códigos generados.

4. **decode** (líneas 200-259 en `huffman.c`):
   - Lee el encabezado y la tabla de frecuencias del archivo comprimido.
   - Reconstruye el árbol Huffman.
   - Decodifica bit por bit, recorriendo el árbol hasta encontrar un nodo hoja.
   - Escribe los símbolos decodificados en el archivo de salida.

### 3. Operaciones de Buffer de Archivos

En `file_buffer.c`, se hacen las operaciones para trabajar con archivos:

1. **create_file_buffer_ops** (líneas 47-69): 
   - Crea una estructura `buffer_ops` para un archivo.
   - Implementa las operaciones eof, read, write y rewind para archivos.

   - eof: Verifica si se ha alcanzado el final del archivo. 
   - read: Lee datos de un archivo hacia la memoria. 
   - write: Escribe datos desde la memoria hacia un archivo.
   - rewind: Se reposiciona el puntero del archivo al inicio del archivo.

2. **desotry_file_buffer_ops** (líneas 71-76):
   - Libera los recursos asociados con un buffer de archivo.

### 4. Operaciones de Buffer de Memoria

Een el `mem_buffer.c`, se hacen las operaciones para trabajar con memoria:

1. **create_mem_buffer_ops** (líneas 46-58):
   - Crea una estructura `buffer_ops` para un buffer en memoria.
   - Implementa las operaciones eof, read, write y rewind para memoria.

2. **desotry_mem_buffer_ops** (líneas 60-62):
   - Libera los recursos asociados con un buffer de memoria.

### 5. main.c

1. **Modo de limpieza** (líneas 66-106):
   - Se activa con el argumento `-empty`.
   - Vacía los directorios `librosHzip` y `librosHunzip` a la papelera.

2. **Modo de ejecución estándar** (líneas 107-217):
   - Comprime todos los archivos .txt en `libros/` a un único archivo `librosHzip/archivolibros.huff`.
   - Descomprime ese archivo en `librosHunzip/`.
   - Mide y printea los tiempos de compresión y descompresión.

#### Funciones Auxiliares (esto lo traía el código del repo original y creo que lo podemos quitar para simplificar)

ya lo intente simplificar un poco y eliminé varias lineas en mi rama

1. **file_encode** (líneas 18-34):
   - Codifica un archivo de entrada a un archivo de salida usando los buffer_ops.

2. **file_decode** (líneas 36-52):
   - Decodifica un archivo comprimido a un archivo de salida usando buffer_ops.

## Flujo de Ejecución

### Flujo de Compresión

1. Se abre el directorio `libros/` (líneas 121-129).
2. Se crea un archivo único de salida `librosHzip/archivolibros.huff` (líneas 113-120).
3. Para cada archivo .txt encontrado (líneas 130-165):
   - Se lee el tamaño del archivo de entrada.
   - Se codifica el archivo a un buffer en memoria utilizando el algoritmo Huffman.
   - Se escribe la metadata (nombre y tamaño) y los datos codificados al archivo único.

### Flujo de Descompresión

1. Se abre el archivo comprimido `librosHzip/archivolibros.huff` (líneas 174-179).
2. Se itera a través de los archivos comprimidos en el archivo único (líneas 180-211):
   - Se lee la longitud del nombre del archivo.
   - Se lee el nombre del archivo.
   - Se lee el tamaño de los datos comprimidos.
   - Se leen los datos comprimidos en un buffer.
   - Se decodifican los datos y se escriben en el archivo de salida correspondiente en `librosHunzip/`.

## Funcionamiento del Algoritmo

### Proceso de Codificación

1. **Análisis de frecuencias** (líneas 127-131 en `huffman.c`):
   - Se cuenta la frecuencia de cada byte en el archivo de entrada.

2. **Construcción del árbol** (líneas 146-147 en `huffman.c`):
   - Se ordenan los símbolos por frecuencia.
   - Se construye el árbol Huffman combinando los nodos de menor frecuencia.

3. **Generación de códigos** (líneas 154-155 en `huffman.c`):
   - Se asignan códigos binarios a cada símbolo según su posición en el árbol.

4. **Escritura de datos comprimidos** (líneas 161-187 en `huffman.c`):
   - Se escribe el encabezado y la tabla de frecuencias.
   - Se codifica cada byte del archivo de entrada utilizando los códigos generados.
   - Se agrupan los bits en bytes para escribirlos en el archivo de salida.

### Proceso de Decodificación

1. **Lectura de metadatos** (líneas 207-219 en `huffman.c`):
   - Se lee el encabezado y la tabla de frecuencias.
   - Se verifica el número mágico para confirmar que es un archivo válido.

2. **Reconstrucción del árbol** (líneas 225-226 en `huffman.c`):
   - Se reconstruye el árbol Huffman a partir de la tabla de frecuencias.

3. **Decodificación de datos** (líneas 232-254 en `huffman.c`):
   - Se recorre el árbol bit por bit según los datos comprimidos.
   - Cuando se llega a un nodo hoja, se escribe el símbolo correspondiente en el archivo de salida.

## Características Técnicas

1. **Encapsulamiento de E/S**: aqui lo que use es una interfaz común (`buffer_ops`) para operaciones de E/S.

2. **Gestión de memoria**: se usia asignación dinámica de memoria para gestionar estructuras de datos como el árbol Huffman y los buffers temporales.

3. **Metadatos de archivo**: El formato de archivo comprimido incluye:
   - Un número mágico ("HUFFMAN") para identificación.
   - El tamaño original del archivo.
   - La tabla de frecuencias necesaria para la reconstrucción del árbol.

4. **Archivo único multifichero**: un formato de archivo único que contiene varios archivos comprimidos, cada uno con su propio nombre y tamaño.

