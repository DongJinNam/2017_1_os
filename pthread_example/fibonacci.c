#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // pthread library 사용

typedef struct {
    int count; // fibonacci 순열의 개수
    unsigned long long *arr; // 자식 쓰레드에서 사용할 sequence array
} thread_params; // 쓰레드 매개변수에 넣을 fibonacci 순열 원소의 개수, sequence

void *fibonacci(void *params); // fibonacci 함수

int main(int argc, char *argv[])
{
    thread_params tp;
    pthread_t pid;
    unsigned long long testArray[60] = {0}; // 피보나치 순열 시퀀스 원소의 개수를 최대 60개로 정한다.
    int count,i;
    void **ret;
    scanf("%d",&count);

    tp.count = count;
    tp.arr = testArray;

    pthread_create(&pid,NULL,fibonacci,&tp); // 자식 쓰레드 생성
    pthread_join(pid,&ret); //자식 쓰레드 waiting

    // sequence 결과값 출력
    for(i = 0; i < count-1; i++)
        printf("%llu,",testArray[i]);
    printf("%llu\n",testArray[count-1]);
    return 0;
}

void *fibonacci(void *params) {
    thread_params *tp = (thread_params *) params;
    int i;
    tp->arr[0] = 0;
    tp->arr[1] = 1;
    if (tp->count > 2) {
        for (i = 2; i < tp->count; i++)
            tp->arr[i] = tp->arr[i-1] + tp->arr[i-2];
    }
    pthread_exit(NULL); // 쓰레드 종료
}
