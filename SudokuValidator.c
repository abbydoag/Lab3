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

//verificar filas (9x9)
bool ver_row(){
    for (int i = 0; i<SUDOKU_SIZE; i++){
        bool seen[SUDOKU_SIZE +1] = {false};
        for (int j = 0; j<SUDOKU_SIZE; j++){
            int num = sudoku[i][j];
            //no valido
            if (num <1 || num > 9 || seen[num]){
                return false;
            }
            seen[num] = true;
        }
    }
    return true;
}

//verificar columnas (9x9)
void* ver_col(void* arg){
    printf("Thread (columnas): %Ild \n", syscall(SYS_gettid));
    for( int j = 0; j< SUDOKU_SIZE; j++){
        bool seen[SUDOKU_SIZE+1] = {false};
        for(int i = 0; i<SUDOKU_SIZE; i++){
            int num =  sudoku[i][j];
            if (num < 1|| num > 9 || seen[num]){
                pthread_exit((void*) false);
            }
            seen[num] = false;
        }
    }
    pthread_exit((void*) true);
}

//subarreglo de 3x3 dentro de uno de 9x9
bool ver_sub(int start_row, int start_col) {
    bool seen[SUDOKU_SIZE + 1] = {false}; // 1-9
    for (int i = start_row; i < start_row + 3; i++) {
        for (int j = start_col; j < start_col + 3; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || seen[num]) {
                return false;
            }
            seen[num] = true;
        }
    }
    return true;
}

int main(int argc, char* argv[]){
    if(argc !=2){
        fprintf(stderr, "Archivo: %s<archivo_sudoku>", argv[0]);
        return 1;
    }

    //leer archivo para sudoku
    int archivo = open(argv[1], O_RDONLY);
    if (archivo == -1){
        perror("🚫 ERROR, NO SE PUDO ABRIR EL ARCHIVO 🚫");
        return 1;
    }

    struct stat st;
    if(fstat(archivo, &st)){
        perror("🚫 ERROR AL OBTENER TAMANO DE ARCHIVO 🚫");
        close(archivo);
        return 1;
    }

    char* datos = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, archivo, 0);
    if (datos == MAP_FAILED){
        perror("🚫 ERROR AL MAPEAR ARCHIVO 🚫");
        close(archivo);
        return 1;
    }

    //copiar a matriz sudoku[9][9]
    for(int i = 0; i<SUDOKU_SIZE; i++){
        for(int j = 0; j<SUDOKU_SIZE; j++){
            sudoku[i][j] = datos[i * SUDOKU_SIZE + j]- '0'; //car -> int
        }
    }

    munmap(datos, st.st_size);
    close(archivo);
    printf("SUDOKU:\n");
    for (int i = 0; i < SUDOKU_SIZE; i++) {
        for (int j = 0; j < SUDOKU_SIZE; j++) {
            printf("%d ", sudoku[i][j]);
        }
        printf("\n");
        
    }
    printf("-----------------\n");

    //3x3
    for(int i = 0; i<SUDOKU_SIZE; i+=3){
        for(int j = 0; j<SUDOKU_SIZE; j+=3){
            if(!ver_sub(i,j)){
                printf("Subarreglo 3x3 en (%d,%d) invaido\n", i,j);
                return 1;
            }
        }
    }

    pthread_t thread;
    pthread_create(&thread, NULL, ver_col, NULL);

    //proceos (numero proceso, fork)
    pid_t pid = fork();
    if(pid == 0){
        //ps –p <#proc> -lLf
        char cmd[100];
        sprintf(cmd, "ps -p %d -lf", getppid());
        system(cmd);
        exit(0);
    }else{
        void* col_valid;
        pthread_join(thread, &col_valid);
        printf("Thread id (main): %ld \n", syscall(SYS_gettid));

        wait(NULL);

        bool row_valid = ver_row();

        if((bool)col_valid && row_valid){
            printf("\n ✅ SUDOKU VALIDO ✅ \n");
        }else{
            printf("\n 🚫 SUDOKU NO VALIDO 🚫 \n");
        }

        //ver lwp antes de terminar
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