#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <omp.h>

#define SUDOKU_SIZE 9 // 9x9 = 81 digitos

//matriz sudoku

int sudoku[SUDOKU_SIZE][SUDOKU_SIZE];
//OPENMP
//verificar filas (9x9)
bool ver_row() {
    omp_set_nested(true);              
    omp_set_num_threads(SUDOKU_SIZE); //9 threads 9 filas

    bool valid = true;
    #pragma omp parallel for shared(valid) schedule(dynamic)
    for (int i = 0; i < SUDOKU_SIZE; i++) {
        bool seen[SUDOKU_SIZE + 1] = {false};
        for (int j = 0; j < SUDOKU_SIZE; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || seen[num]) {
                #pragma omp atomic write //evita race condiiton
                valid = false;
            }
            seen[num] = true;
        }
    }
    return valid;
}

//verificar columnas (9x9)
void* ver_col(void* arg) {
    omp_set_nested(true);               
    omp_set_num_threads(SUDOKU_SIZE); //9 threads 9 columnas

    printf("Thread (columnas): %ld\n", syscall(SYS_gettid));
    bool valid = true;
    
    #pragma omp parallel for shared(valid)
    for (int j = 0; j < SUDOKU_SIZE; j++) {
        bool seen[SUDOKU_SIZE + 1] = {false};  // Privada por thread
        for (int i = 0; i < SUDOKU_SIZE; i++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || seen[num]) {
                #pragma omp atomic write
                valid = false;
            }
            seen[num] = true;
        }
    }
    
    pthread_exit((void*)(intptr_t)valid);
}

//subarreglo de 3x3 dentro de uno de 9x9
bool ver_sub(int start_row, int start_col) {
    omp_set_nested(true);               
    omp_set_num_threads(9); //9 celdas 9 threads

    bool seen[SUDOKU_SIZE + 1] = {false};
    bool valid = true;
    
    #pragma omp parallel for collapse(2) shared(valid, seen) schedule(dynamic)
    for (int i = start_row; i < start_row + 3; i++) {
        for (int j = start_col; j < start_col + 3; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9) {
                #pragma omp atomic write
                valid = false;
            }
            #pragma omp critical
            {
                if (seen[num]) {
                    valid = false;
                }
                seen[num] = true;
            }
        }
    }
    return valid;
}

int main(int argc, char* argv[]) {
    omp_set_num_threads(4);
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    // Leer archivo con mmap()
    int archivo = open(argv[1], O_RDONLY);
    if (archivo == -1) {
        perror("Error al abrir el archivo");
        return 1;
    }

    struct stat st;
    if (fstat(archivo, &st)) {
        perror("Error al obtener tamaño del archivo");
        close(archivo);
        return 1;
    }

    char* datos = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, archivo, 0);
    if (datos == MAP_FAILED) {
        perror("Error al mapear el archivo");
        close(archivo);
        return 1;
    }

    // Copiar datos a la matriz sudoku[9][9]
    for (int i = 0; i < SUDOKU_SIZE; i++) {
        for (int j = 0; j < SUDOKU_SIZE; j++) {
            sudoku[i][j] = datos[i * SUDOKU_SIZE + j] - '0';
        }
    }

    munmap(datos, st.st_size);
    close(archivo);

    // Mostrar sudoku cargado
    printf("SUDOKU:\n");
    for (int i = 0; i < SUDOKU_SIZE; i++) {
        for (int j = 0; j < SUDOKU_SIZE; j++) {
            printf("%d ", sudoku[i][j]);
        }
        printf("\n");
    }
    printf("-----------------\n");

    // Verificar submatrices 3x3
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < SUDOKU_SIZE; i += 3) {
        for (int j = 0; j < SUDOKU_SIZE; j += 3) {
            if (!ver_sub(i, j)) {
                printf("Submatriz en (%d,%d) inválida\n", i, j);
                exit(1);
            }
        }
    }

    pthread_t thread;
    pthread_create(&thread, NULL, ver_col, NULL);

    pid_t pid = fork();
    if (pid == 0) {
        char cmd[100];
        sprintf(cmd, "ps -p %d -lf", getppid());
        system(cmd);
        exit(0);
    } else {
        void* col_valid_ptr;
        pthread_join(thread, &col_valid_ptr);
        bool col_valid = (bool)(intptr_t)col_valid_ptr;
        
        printf("Thread ID (main): %ld\n", syscall(SYS_gettid));

        wait(NULL);

        bool row_valid = ver_row();

        if (col_valid && row_valid) {
            printf("\n✅ SUDOKU VALIDO ✅\n");
        } else {
            printf("\n🚫 SUDOKU NO VALIDO 🚫\n");
        }

        pid_t pid2 = fork();
        if (pid2 == 0) {
            char cmd[100];
            sprintf(cmd, "ps -L -p %d -lf", getppid());
            system(cmd);
            exit(0);
        } else {
            wait(NULL);
        }
    }

    return 0;
}