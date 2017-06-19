#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <stdbool.h>

#define TLB_SIZE 16 // TLB Entry Size
#define PT_MAXCOUNT 256 // Page Table 최대 Entry 개수
#define FT_MAXCOUNT 128 // Physical Memory 최대 Entry 개수
#define PAGE_UNIT 256 // BACKING STORE에서 읽을 page 단위 bits 수

struct Node {
    int data;
    struct Node *prev;
    struct Node *next;
} Node; // LRU Algorithm 내에서 사용할 stack에서의 Node struct type

struct Node *headOfTLB = NULL, *tailOfTLB = NULL; // TLB LRU Stack을 Doubly linked list 형태로 구현하기 위해.
struct Node *headOfMemory = NULL, *tailOfMemory = NULL; // Memory LRU Stack을 Doubly linked list 형태로 구현하기 위해.

int cntTLBEntries = 0; // TLB에 있는 entry 개수
int cntLoadedFromBS = 0; // backing store로부터 읽어들인 데이터 개수
int cntPFWithLRU = 0; //page faults count(Page Replacement policy : LRU)
int cntPFWithFIFO = 0; //page faults count(Page Replacement policy : FIFO)
int cntTLBHitsWithLRU = 0; // TLB 적중 횟수(Page Replacement policy : LRU)
int cntTLBHitsWithFIFO = 0; // TLB 적중 횟수(Page Replacement policy : FIFO)
int cntLRUHits = 0; // LRU 적중 횟수
int cntFIFOHits = 0; // FIFO 적중 횟수
int cntTranslatedAddress = 0; // 변환된 주소의 개수
int howTo = 0; // Page Table에 대해 LRU policy : 1, FIFO policy : 2

FILE *BS = NULL; // 입력할 BACKING_STORE.bin file
FILE *addresses = NULL; // 입력할 addresses.txt file
FILE *physical_lru = NULL; // 출력할 Physical_LRU.txt file
FILE *frame_table_lru = NULL; // 출력할 Frame_Table_LRU.txt file
FILE *p_memory_lru = NULL; // 출력할 Physical_Memory_LRU.bin file
FILE *physical_fifo = NULL; // 출력할 Physical_FIFO.txt file
FILE *frame_table_fifo = NULL; // 출력할 Frame_Table_FIFO.txt file
FILE *p_memory_fifo = NULL; // 출력할 physical_memory_fifo.txt file

//int physicalMemory[FT_MAXCOUNT][PAGE_UNIT]; // memory
unsigned char physicalMemory[FT_MAXCOUNT][PAGE_UNIT]; // memory
int frameNumArr[PT_MAXCOUNT]; // page table frame num
int offsetArr[PT_MAXCOUNT]; // page offset array

int TLB[TLB_SIZE][2]; // col 0 : page Num, col 1 : frame Num

void getPage(int log_addr); // logical address로부터 page를 얻어오는 함수
int readFromBS(int pageNum, int offset); // BACKING_STORE로부터 256byte를 읽어오는 함수 return 값은 frame number
void insertIntoTLB(int pageNum, int frameNum); // TLB에 pageNum, frameNum을 넣는 함수 (LRU Algorithm 사용합니다)

struct Node *getNewNode(int x); // Doubly linked list의 데이터 x값을 가진 node를 만드는 함수
void insertAt(struct Node *tail, int x);//tail Node 앞에 데이터 x값을 가진 node를 만들어서 list에 넣는 함수
void removeAt(struct Node *head, int x);//head Node 부터 시작해서, 데이터 값이 x인 노드를 찾아 list에서 제거하는 함수
int removeFrontAt(struct Node *head, struct Node *tail); // list 맨앞에 있는 노드를 제거한다.
void clearList(struct Node *head, struct Node *tail); // list 에 있는 head, tail 노드를 제외한 모든 노드를 제거한다.

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,"Usage : [file name] [addresses.txt]\n");
        return -1;
    }

    char linear_address[10]; // 한줄마다 입력받는 linear address
    int integer_addr = 0, i, j, k; // int type address, loop iterators

    physical_lru = fopen("Physical_LRU.txt","w");
    p_memory_lru = fopen("Physical_Memory_LRU.bin","w");
    frame_table_lru = fopen("Frame_Table_LRU.txt","w");
    physical_fifo = fopen("Physical_FIFO.txt","w");
    p_memory_fifo = fopen("Physical_Memory_FIFO.bin","w");
    frame_table_fifo = fopen("Frame_Table_FIFO.txt","w");
    BS = fopen("BACKING_STORE.bin","r");

    if (BS == NULL) {
        fprintf(stderr,"Backing store file can't opened\n");
        return -1;
    }

    for (i = 0; i < 2; i++) {
        howTo = i + 1; // howTo : 1 - LRU, howTo : 2 - FIFO
        addresses = fopen(argv[1],"r");

        // LRU 기준 테스트
        memset(physicalMemory,0,sizeof(unsigned char)*FT_MAXCOUNT*PAGE_UNIT);
        memset(TLB,0,sizeof(int)*TLB_SIZE*2);        
        for (j = 0; j < PT_MAXCOUNT; j++) {
            frameNumArr[j] = -1; //invalid
            offsetArr[j] = -1; // initialize to -1 (비어있음을 의미)
        }

        headOfTLB = getNewNode(-1);
        tailOfTLB = getNewNode(-1);
        headOfTLB->next = tailOfTLB;
        tailOfTLB->prev = headOfTLB;

        if (howTo == 1) {
            headOfMemory = getNewNode(-1);
            tailOfMemory = getNewNode(-1);
            headOfMemory->next = tailOfMemory;
            tailOfMemory->prev = headOfMemory;
            cntLRUHits = 0;            
            cntTLBHitsWithLRU = 0;
            cntPFWithLRU = 0;
        }
        else {
            cntFIFOHits = 0;
            cntTLBHitsWithFIFO = 0;
            cntPFWithFIFO = 0;
        }
        cntLoadedFromBS = 0;
        cntTranslatedAddress = 0; // 변환된 주소 개수        
        cntTLBEntries = 0; // TLB 엔트리 개수

        while (fgets(linear_address,10,addresses) != NULL) {
            integer_addr = atoi(linear_address);
            getPage(integer_addr);
            cntTranslatedAddress++;
        }

        // TLB Node 모두 초기화
        //Memory LRU 알고리즘에서 사용한 모든 Node 제거

        if (howTo == 1) {
            clearList(headOfMemory,tailOfMemory);
            if (headOfMemory) free(headOfMemory); headOfMemory = NULL;
            if (tailOfMemory) free(tailOfMemory); tailOfMemory = NULL;
        }

        // frame table 출력
        for (j = 0; j < FT_MAXCOUNT; j++) {
            bool bFind = false;
            for (k = 0; k < PT_MAXCOUNT; k++) {
                if (frameNumArr[k] == j) {
                    bFind = true;
                    break;
                }
            }
            if (howTo == 1) {                
                fprintf(frame_table_lru,"%d ",j);
                // 해당 frame에 맞는 page number가 없을 시에, invalid.
                fprintf(frame_table_lru,"%d ",bFind == true ? 1 : 0);
                if (bFind == true)
                    fprintf(frame_table_lru,"%d\n", k << 8 | offsetArr[k]);
                else
                    fprintf(frame_table_lru,"\n");
            }
            else {
                fprintf(frame_table_fifo,"%d ",j);
                // 해당 frame에 맞는 page number가 없을 시에, invalid.
                fprintf(frame_table_fifo,"%d ",bFind == true ? 1 : 0);
                if (bFind == true)
                    fprintf(frame_table_fifo,"%d\n", k << 8 | offsetArr[k]);
                else
                    fprintf(frame_table_fifo,"\n");
            }
        }

        // physical memory 출력
        for (j = 0; j < FT_MAXCOUNT; j++) {
            for (k = 0; k < PAGE_UNIT; k++) {
                if (howTo == 1)
                    fprintf(p_memory_lru,"%c",physicalMemory[j][k]);
                else
                    fprintf(p_memory_fifo,"%c",physicalMemory[j][k]);
            }
        }

        //TLB LRU 알고리즘에서 사용한 모든 Node 제거
        clearList(headOfTLB,tailOfTLB);
        if (headOfTLB) free(headOfTLB); headOfTLB = NULL;
        if (tailOfTLB) free(tailOfTLB); tailOfTLB = NULL;

        fclose(addresses);        
    }
    // Page Replacement LRU로 Simulation 할 시에 결과
    printf("TLB hit ratio : %d hits out of %d\n", cntTLBHitsWithLRU, cntTranslatedAddress);
    printf("LRU hit ratio : %d hits out of %d\n", cntLRUHits, cntTranslatedAddress);
    // Page Replacement FIFO로 Simulation 할 시에 결과
    printf("TLB hit ratio : %d hits out of %d\n", cntTLBHitsWithFIFO, cntTranslatedAddress);
    printf("FIFO hit ratio : %d hits out of %d\n", cntFIFOHits, cntTranslatedAddress);
    fclose(physical_lru);
    fclose(p_memory_lru);
    fclose(frame_table_lru);
    fclose(physical_fifo);
    fclose(p_memory_fifo);
    fclose(frame_table_fifo);
    fclose(BS);
    return 0;
}

void getPage(int log_addr) {
    int pageNum = (log_addr >> 8) & 0xFF; // page Number
    int frameNum = -1;
    int offset = log_addr - (pageNum << 8); // offset
    int i, index;

    // TLB에 page number가 있는지 check.
    for (i = 0; i < TLB_SIZE; i++) {
        if (TLB[i][0] == pageNum) {
            index = i;
            frameNum = TLB[i][1];
            // TLB hit 성공
            if (howTo == 1) cntTLBHitsWithLRU++;
            else cntTLBHitsWithFIFO++;

            offsetArr[pageNum] = offset; // offset update
            break;
        }
    }

    // TLB hit에 실패했을 시
    if (frameNum == -1) {
        // page table 검사
        frameNum = frameNumArr[pageNum];
        // page table에도 없는 경우, 실제로 Backing store에서 데이터를 가져옵니다.
        if (frameNum < 0) {            
            frameNum = readFromBS(pageNum, offset); // BACKING_STORE로부터 가져온다.
            //offsetArr[pageNum] = offset;
            if (howTo == 1) cntPFWithLRU++;
            else cntPFWithFIFO++;
        }
        else {
            // page table에 있는 경우, hit!
            if (howTo == 1) {
                removeAt(headOfMemory,frameNum);
                insertAt(tailOfMemory,frameNum);                
                cntLRUHits++;
            }
            else {
                cntFIFOHits++;
            }
            offsetArr[pageNum] = offset; // offset update
        }        
    }
    else { // TLB 적중 성공 시
        if (howTo == 1) {
            // Page 교체 알고리즘이 LRU일 시에, Page Table Update
            removeAt(headOfMemory,frameNum);
            insertAt(tailOfMemory,frameNum);
        }
    }
    insertIntoTLB(pageNum,frameNum); // pageNum과 frameNum 값을 TLB에 넣고, Stack을 Update한다.
    if (howTo == 1) // Physical.txt 출력
        fprintf(physical_lru,"Virtual address: %d Physical address: %d\n", log_addr,(frameNum << 8) | offset);
    else
        fprintf(physical_fifo,"Virtual address: %d Physical address: %d\n", log_addr,(frameNum << 8) | offset);
}

int readFromBS(int pageNum, int offset) {
    int f_offset = PAGE_UNIT*pageNum;
    if (fseek(BS,f_offset,SEEK_SET) != 0) {
        // seek 실패에 대한 예외처리
        fprintf(stderr,"seeking failed in backing store.\n");
        return -1;
    }

    unsigned char page_str[PAGE_UNIT];
    int i, index; // loop iterator, index to remove
    int rtn = -1;

    if (fread(page_str,sizeof(unsigned char),PAGE_UNIT,BS) <= 0) {
        // read 실패에 대한 예외처리
        fprintf(stderr,"reading failed in backing store.\n");
        return -1;
    }

    // page 교체 알고리즘( 1 : LRU, 2 : FIFO)
    if (howTo == 1) {
        if (cntLoadedFromBS < FT_MAXCOUNT) { // 불러온 회수가 128 미만인 경우
            insertAt(tailOfMemory,cntLoadedFromBS);
            frameNumArr[pageNum] = cntLoadedFromBS;
            offsetArr[pageNum] = offset; // offset update
            rtn = cntLoadedFromBS;
        }
        else { // 불러온 회수가 128 이상인 경우 (Page 교체 알고리즘[LRU]를 사용한다)
            index = removeFrontAt(headOfMemory,tailOfMemory); // 교체하려는 index number
            insertAt(tailOfMemory,index);
            int memoryIndex = 0;
            for (i = 0; i < 256; i++) {
                if (frameNumArr[i] == index) {
                    memoryIndex = i;
                    break;
                }
            }
            frameNumArr[memoryIndex] = -1; // frameNum = -1 (page Number에 대한 frame이 메모리에 존재하지 않다는 의미)
            frameNumArr[pageNum] = index;

            offsetArr[memoryIndex] = -1;
            offsetArr[pageNum] = offset; // offset update

            rtn = index;
        }
        // Disk to Physical Memory
        for (i = 0; i < PAGE_UNIT; i++)
            physicalMemory[rtn][i] = page_str[i];
    }
    else {
        if (cntLoadedFromBS < FT_MAXCOUNT) {
            frameNumArr[pageNum] = cntLoadedFromBS;
            offsetArr[pageNum] = offset; // offset update
            rtn = cntLoadedFromBS;
        }
        else { // 불러온 회수가 128 이상인 경우 (Page 교체 알고리즘[FIFO]를 사용한다)
            index = cntLoadedFromBS % FT_MAXCOUNT;
            int memoryIndex = 0;
            for (i = 0; i < 256; i++) {
                if (frameNumArr[i] == index) {
                    memoryIndex = i;
                    break;
                }
            }
            frameNumArr[memoryIndex] = -1;
            frameNumArr[pageNum] = index;

            offsetArr[memoryIndex] = -1;
            offsetArr[pageNum] = offset; // offset update
            rtn = index;
        }
        // Disk to Physical Memory
        for (i = 0; i < PAGE_UNIT; i++)
            physicalMemory[rtn][i] = page_str[i];
    }
    cntLoadedFromBS++; // Backing Store로부터 page를 읽어온 회수
    return rtn;
}

void insertIntoTLB(int pageNum, int frameNum) {
    int i, index;
    bool bExist = false;

    for (i = 0; i < TLB_SIZE; i++) {
        if (TLB[i][0] == pageNum) {
            bExist = true;
            break;
        }
    }
    if (bExist) {
        // TLB LRU Stack Update
        removeAt(headOfTLB,i);
        insertAt(tailOfTLB,i);
    }
    else {
        if (cntTLBEntries < TLB_SIZE) {
            // TLB LRU Stack Update
            insertAt(tailOfTLB,cntTLBEntries);
            TLB[cntTLBEntries][0] = pageNum;
            TLB[cntTLBEntries][1] = frameNum;
        }
        else {
            // TLB LRU Stack Update
            index = removeFrontAt(headOfTLB,tailOfTLB); // 교체하려는 index number
            insertAt(tailOfTLB,index);
            TLB[index][0] = pageNum;
            TLB[index][1] = frameNum;
        }
    }
    if (cntTLBEntries < TLB_SIZE)
        cntTLBEntries++; // TLB 최대 사이즈까지만 증가시키도록 한다.
}

// new node 만들기
struct Node *getNewNode(int x) {
    struct Node *newNode = (struct Node *) malloc(sizeof(struct Node));
    newNode->data = x;
    newNode->prev = NULL;
    newNode->next = NULL;
    return newNode;
}

void insertAt(struct Node *tail, int x) {
    struct Node *newNode = getNewNode(x);
    newNode->next = tail;
    newNode->prev = tail->prev;
    tail->prev->next = newNode;
    tail->prev = newNode;
}

void removeAt(struct Node *head, int x) {
    struct Node *node = head;
    while (node != NULL) {
        if (node->data == x)
            break;
        node = node->next;
    }
    if (node->data == x) {
        struct Node *next = node->next;
        struct Node *prev = node->prev;
        prev->next = next;
        next->prev = prev;
        if (node) free(node); node = NULL;
    }
}

int removeFrontAt(struct Node *head, struct Node *tail) {
    int front = -1;
    if (head->next != tail) {
        struct Node *node = head->next;
        struct Node *prev = NULL, *next = NULL;
        prev = node->prev;
        next = node->next;
        prev->next = next;
        next->prev = prev;
        front = node->data;
        if (node) free(node); node = NULL;
    }
    return front;
}

void clearList(struct Node *head, struct Node *tail) {
    struct Node *temp = head->next, *next = NULL;
    while (temp != tail) {
        next = temp->next;
        free(temp);
        temp = next;
    }
}
