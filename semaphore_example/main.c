#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE 10
typedef int buffer_item;

typedef struct {
    sem_t empty;  // semaphore empty 초기값 BUFFER_SIZE
    sem_t full; // semaphore full 초기값 0
    pthread_mutex_t mutex; // mutex lock
    sem_t pd_sem; // produce monitoring semaphore lock-1 초기값 0
    sem_t pd_sem2; // produce monitoring semaphore lock-2 초기값 0
    sem_t cs_sem;// consume monitoring semaphore lock-1 초기값 0
    sem_t cs_sem2;// consume monitoring semaphore lock-2 초기값 0
    int ins_value; // insert 하려는 value 값
    int rmv_value; // remove 하려는 value 값
    int count; // buffer item's count
    int in; // buffer[in%BUFFER_SIZE] is the first empty slot
    int out; // buffer[out%BUFFER_SIZE] is the first full slot
    buffer_item buffer[BUFFER_SIZE]; // bounded buffer
} ProducerConsumer; // producer consumer problem 관리 struct

ProducerConsumer shared;// monitor instance

void *p_monitoring(void *param); // (producer thread) monitoring thread
void *c_monitoring(void *param); // (consumer thread) monitoring thread
void *producer(void *param); // producer thread
void *consumer(void *param); // consumer thread
int insert_item(buffer_item item); // bounded buffer에 item을 넣는 함수
int remove_item(buffer_item *item); // bounded buffer로 부터 item을 제거하는 함수

int main(int argc, char *argv[])
{
    int t_sleep,cnt_pt,cnt_ct,i;
    pthread_t p_t[20],c_t[20],p_m,c_m; // producer thread id 배열, consumer thread id 배열, producer monitoring pthread_id, consumer monitoring pthread_id,
    pthread_attr_t attr;

    if (argc != 4) {
        printf("usage : <file_name> <how long to sleep before terminate> <# of producer threads> <# of consumer threads>\n");
        return 0;
    }

    shared.count = 0; // 버퍼의 데이터 값이 0개이기 때문에 0 으로 초기화
    shared.in = 0; // in index
    shared.out = 0; // out index

    t_sleep = atoi(argv[1]); // producer, consumer thread 생성 후 대기 시간
    cnt_pt = atoi(argv[2]); // producer thread 개수
    cnt_ct = atoi(argv[3]); // consumer thread 개수
    // create the mutex lock
    pthread_mutex_init(&shared.mutex,NULL);
    pthread_attr_init(&attr);
    // create full and empty semaphore
    sem_init(&shared.empty,0,BUFFER_SIZE);
    sem_init(&shared.full,0,0);
    sem_init(&shared.pd_sem,0,0); // producer thread 와 producer monitoring thread 간 사용할 semaphore
    sem_init(&shared.cs_sem,0,0); // consumer thread 와 consumer monitoring thread 간 사용할 semaphore
    sem_init(&shared.pd_sem2,0,0); // producer thread 와 producer monitoring thread 간 사용할 semaphore
    sem_init(&shared.cs_sem2,0,0); // consumer thread 와 consumer monitoring thread 간 사용할 semaphore

    pthread_create(&p_m,&attr,p_monitoring,NULL); // producer monitoring thread
    pthread_create(&c_m,&attr,c_monitoring,NULL); // consumer monitoring thread
    for (i = 0; i < cnt_pt; i++) // producer thread
        pthread_create(&p_t[i],&attr,producer,(void *)&t_sleep); // producer thread 내 sleep time을 rand() % (t_sleep / 2) 으로 합니다.
    for (i = 0; i < cnt_ct; i++) // consumer thread
        pthread_create(&c_t[i],&attr,consumer,(void *)&t_sleep); // consumer thread 내 sleep time을 rand() % (t_sleep / 2) 으로 합니다.

    sleep(t_sleep); // t_sleep 만큼 sleep 함수 수행 후 thread 종료

    // thread 종료 (producer, consumer, producer_monitoring, consumer_monitoring)
    for (i = 0; i < cnt_pt; i++)
        pthread_cancel(p_t[i]);
    for (i = 0; i < cnt_ct; i++)
        pthread_cancel(c_t[i]);
    pthread_cancel(c_m);
    pthread_cancel(p_m);
    return 0;
}

// producer thread
void *producer(void *param) {
    buffer_item item;
    int divisor = *((int *)param);
    while(1) {
        // sleep for a random period of time
        sleep(rand()%divisor);

        sem_wait(&shared.empty);
        pthread_mutex_lock(&shared.mutex);

        // generate a random number between 1 and 100
        item = rand() % 100 + 1;
        shared.ins_value = item;

        sem_post(&shared.pd_sem2); // producer monitoring thread 재개
        sem_wait(&shared.pd_sem); // producer monitoring thread가 끝날 때까지 wait

        // monitoring thread에서 shared.ins_value 값이 -1인 경우 insert 취소, ins_value 값이 item 값인 경우 insert
        if (shared.ins_value > 0) {
            if (insert_item(item))
                fprintf(stderr,"report error condition\n");
            else
                printf("producer produced %d\n",item);
            pthread_mutex_unlock(&shared.mutex);
            sem_post(&shared.full); // consumer thread 중 하나가 item을 consume 하도록 full semaphore 값 증가
        }
        else {
            printf("item insertion failed.\n");
            pthread_mutex_unlock(&shared.mutex);
            sem_post(&shared.empty); // insert 되지 않은 경우에는 empty semaphore 값을 이전 값으로 바꾼다.
        }
    }
}

// consumer thread
void *consumer(void *param) {
    buffer_item item;
    int divisor = *((int *)param);
    while(1) {
        // sleep for a random period of time
        sleep(rand()%divisor);

        sem_wait(&shared.full);
        pthread_mutex_lock(&shared.mutex);

        shared.rmv_value = -1;
        sem_post(&shared.cs_sem2); // consumer monitoring thread 재개
        sem_wait(&shared.cs_sem); // consumer monitoring thread가 끝날 때까지 wait

        // monitoring 통과 시, 제거하려는 값에 문제가 없으면 rmv_value는 제거하려는 아이템의 값.
        // 제거하려는 값의 범위가 25 보다 클 시에는 rmv_value는 -1 값이 설정됩니다.
        if (shared.rmv_value > 0) {
            if (remove_item(&item))
                fprintf(stderr,"report error condition\n");
            else {
                shared.rmv_value = item;
                item = shared.rmv_value;
                printf("consumer consumed %d\n",item);
            }
            pthread_mutex_unlock(&shared.mutex);
            sem_post(&shared.empty); // producer thread 중 하나가 item을 produce 하도록 empty semaphore 값 증가
        }
        else {
            printf("buffer output item is divided by 2.\n");
            pthread_mutex_unlock(&shared.mutex);
            sem_post(&shared.full); // remove 되지 않은 경우에는 full semaphore 값을 이전 값으로 바꾼다.
        }
    }
}

// bounded buffer에 item을 삽입하는 함수
int insert_item(buffer_item item) {
    if (shared.count >= BUFFER_SIZE)
        return -1;
    else {
        shared.buffer[shared.in] = item;
        shared.in = (shared.in+1) % BUFFER_SIZE;
        shared.count++; // 버퍼 아이템 개수 증가
        return 0;
    }
}

// bounded buffer에 item을 제거하는 함수
int remove_item(buffer_item *item) {
    if (shared.count <= 0)
        return -1;
    else {
        int nextProduced = shared.buffer[shared.out];
        shared.buffer[shared.out] = 0;
        shared.out = (shared.out+1) % BUFFER_SIZE;
        shared.count--; // 버퍼 아이템 개수 감소
        *item = nextProduced;
        return 0;
    }
}

// producer monitoring thread
void *p_monitoring(void *param) {
    while(1) {
        sem_wait(&shared.pd_sem2); // producer thread에서 sem_post(&shared.pd_sem2) 수행 후 monitoring thread 재개
        // insert 하려는 item 값이 1 이상, 50 이하 범위가 아닌 경우
        // producer thread에서 ins_value = -1 로 수정해서 insert 하지 않도록 한다.
        if (shared.ins_value < 1 || shared.ins_value > 50)
            shared.ins_value = -1; // 모니터링 조건 실패
        sem_post(&shared.pd_sem); // monitoring 후에는 producer thread를 재개시키도록 한다.
    }
}

// consumer monitoring thread
void *c_monitoring(void *param) {
    while(1) {
        sem_wait(&shared.cs_sem2); // consumer thread에서 sem_post(&shared.cs_sem2) 수행 후 monitoring thread 재개
        // remove 하려는 item 값이 1 이상 25 이하인 경우, 정상적으로 제거
        // 26 이상 50 이하인 경우, 제거 대상 숫자값을 2로 나눈다.
        if (shared.buffer[shared.out] >= 1 && shared.buffer[shared.out] <= 25)
            shared.rmv_value = shared.buffer[shared.out];
        else {
            shared.buffer[shared.out] >>= 1; // 제거하려는 item 값을 2로 나눈다.
            shared.rmv_value = -1; // 모니터링 조건 실패
        }
        sem_post(&shared.cs_sem); // monitoring 후에는 consumer thread를 재개시키도록 한다.
    }
}
