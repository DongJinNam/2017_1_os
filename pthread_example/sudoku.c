#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // pthread library 사용

#define THREAD_COUNT 11 // main 에서 사용할 thread 개수

typedef struct {
    int row; // Left,Top의 행 index
    int col; // Left,Top의  index
    int (*board)[9]; // sudoku board pointer
}   thread_params;

void *check_rows(void *params); // 행 체크
void *check_cols(void *params); // 열 체크
void *check_square(void *params); // 사각형 체크
void setParams(thread_params *param,int row,int col,int (*board)[9]); // param 값 settting

int main(int argc, char *argv[])
{
    int arr[9][9]; // 입력받을 sudoku 9*9 int 2차원 배열
    int i,j; // 단일 루프 iterator, 이중 루프 iterator
    FILE *file = NULL;
    char buffer[18]; // 입력 시 line 별로 Read 할 버퍼
    pthread_t pt_row,pt_col; // 행 검사 thread id, 열 검사 thread id
    pthread_t pt_arr[9]; // square 연산 pthread id 배열
    thread_params prm_row, prm_col; // 행,열 검사 thread에 넘겨줄 parameter
    thread_params prm_arr[9]; // thread에 넘겨줄 parameter 모음 (square 연산에 관한)
    void *ret_row,*ret_col; // thread 결과 return 값
    void *ret_square;
    int total = 0; // 성공적으로 완료한 thread 개수

    if (argc != 2) { // 매개변수가 2개가 아닐 시에
        printf("Usage : <%s> <file_name>\n",argv[0]);
        return 1;
    }

    file = fopen(argv[1],"r");
    if (file == NULL) return 1;

    i = 0;
    //Input File에 있는 숫자들을 line 별로 입력받아, int 형 2차원 배열에 넣기
    while (fread(buffer,1,18,file)==18) {
        for (j = 0; j < 9; j++)
            arr[i][j] = (int) buffer[j << 1] - '0';
        i++;
    }

    // param setting
    setParams(&prm_row,0,0,arr);
    setParams(&prm_col,0,0,arr);
    setParams(&prm_arr[0],0,0,arr);
    setParams(&prm_arr[1],0,3,arr);
    setParams(&prm_arr[2],0,6,arr);
    setParams(&prm_arr[3],3,0,arr);
    setParams(&prm_arr[4],3,3,arr);
    setParams(&prm_arr[5],3,6,arr);
    setParams(&prm_arr[6],6,0,arr);
    setParams(&prm_arr[7],6,3,arr);
    setParams(&prm_arr[8],6,6,arr);

    // pthread_create
    pthread_create(&pt_row,NULL,check_rows,(void *) &prm_row);
    pthread_create(&pt_col,NULL,check_cols,(void *) &prm_col);
    for (i = 0; i < 9; i++) {
        pthread_create(&pt_arr[i],NULL,check_square,(void *) &prm_arr[i]);
    }

    // pthread_join
    pthread_join(pt_row,&ret_row);
    total += (int) ret_row;
    pthread_join(pt_col,&ret_col);
    total += (int) ret_col;
    for (i = 0; i < 9; i++) {
        pthread_join(pt_arr[i],&ret_square);
        total += (int) ret_square;
    }

    if (total == THREAD_COUNT) // 성공한 thread의 개수가 목표치에 도달했을 때
        printf("Valid result!\n");
    else
        printf("Invalid result!\n");

    fclose(file);
    return 0;
}

// thread parameter data structure pointer 설정
void setParams(thread_params *param, int row, int col, int (*board)[9]) {
    param->row = row;
    param->col = col;
    param->board = board;
}

// 모든 행에 대한 sudoku error check thread
void *check_rows(void *params) {
    thread_params *tp = (thread_params *) params;
    int startR = tp->row;
    int startC = tp->col;
    int i,j,data;
    int success = 1;
    int num[9] = {0};// 각 숫자마다 이미 발견했을 시 1로 표시, 그 전엔 모든 숫자별로 0으로 초기화

    for (i = startR; i < 9; i++) {
        if (success == 0) break;
        for (j = 0; j < 9; j++)
            num[j] = 0;
        for (j = startC; j < 9; j++) {
            data = tp->board[i][j];
            if (num[data-1] > 0) {
                success = 0;
                break;
            }
            else {
                num[data-1] = 1;
            }

        }
    }
    pthread_exit((void *)success);
}

// 모든 열에 대한 sudoku error check thread
void *check_cols(void *params) {
    thread_params *tp = (thread_params *) params;
    int startR = tp->row;
    int startC = tp->col;
    int i,j,data;
    int success = 1;
    int num[9] = {0};// 각 숫자마다 이미 발견했을 시 1로 표시, 그 전엔 모든 숫자별로 0으로 초기화

    for (j = startC; j < 9; j++) {
        if (success == 0) break;
        for (i = 0; i < 9; i++)
            num[i] = 0;
        for (i = startR; i < 9; i++) {
            data = tp->board[i][j];
            if (num[data-1] > 0) {
                success = 0;
                break;
            }
            else {
                num[data-1] = 1;
            }

        }
    }
    pthread_exit((void *)success);
}

//모든 subgrid에 대한 sudoku error check thread
void *check_square(void *params) {
    thread_params *tp = (thread_params *) params;
    int startR = tp->row;
    int startC = tp->col;
    int i,j,data;
    int success = 1;
    int num[9] = {0}; // 각 숫자마다 이미 발견했을 시 1로 표시, 그 전엔 모든 숫자별로 0으로 초기화

    for (i = startR; i < startR+3; i++) {
        if (success == 0) break;
        for (j = startC; j < startC+3; j++) {
            data = tp->board[i][j];
            if (num[data-1] > 0) {
                success = 0;
                break;
            }
            else {
                num[data-1] = 1;
            }

        }
    }
    pthread_exit((void *)success);
}
