#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>

// 控制台颜色
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"

// 全局变量
int M; // 理发师数量
int N; // 座位数量
int CUSTOMER_COUNT; // 顾客数量

// 环形队列
int* waitingQueue; // 排队等待的顾客队列
int queueFront = 0, queueRear = 0, queueSize = 0; // 队列指针和大小
int* seatQueue; // 座位上的顾客队列
int seatCount = 0; // 当前座位人数

// 同步机制
sem_t availableSeats; // 空闲座位计数信号量
sem_t barberReady;    // 理发师空闲信号量
pthread_mutex_t mutex; // 用于保护共享资源的互斥锁

// 理发师完成的理发任务计数器
int barberTasksCompleted = 0;
pthread_mutex_t taskCounterMutex; // 保护任务计数器的互斥锁

struct timeval startTime;

// 获取当前时间（毫秒）
long getCurrentTime() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return (currentTime.tv_sec - startTime.tv_sec) * 1000 + (currentTime.tv_usec - startTime.tv_usec) / 1000;
}

// 模拟时钟函数
void delay(int ms) {
    usleep(ms * 1000); // 毫秒延迟
}

// 环形队列操作（等待队列）
void enqueue(int customerId) {
    waitingQueue[queueRear] = customerId;
    queueRear = (queueRear + 1) % CUSTOMER_COUNT;
    queueSize++;
}

int dequeue() {
    if (queueSize > 0) {
        int customerId = waitingQueue[queueFront];
        queueFront = (queueFront + 1) % CUSTOMER_COUNT;
        queueSize--;
        return customerId;
    }
    return -1; // 队列为空
}

// 理发师线程
void* barber_thread(void* arg) {
    int barberId = *(int*)arg;
    while (1) {
        // 等待有顾客准备理发
        sem_wait(&barberReady);

        pthread_mutex_lock(&mutex);
        // 处理座位上的顾客
        int customerId = seatQueue[--seatCount]; // 从座位中获取顾客 ID
        pthread_mutex_unlock(&mutex);

        printf(GREEN "[时钟 %ld ms] 理发师 %d 开始为顾客 %d 理发。\n" RESET, getCurrentTime(), barberId, customerId);

        // 模拟理发时间
        delay(5000);

        printf(GREEN "[时钟 %ld ms] 理发师 %d 完成为顾客 %d 理发。\n" RESET, getCurrentTime(), barberId, customerId);

        // 增加空闲座位
        sem_post(&availableSeats);

        // 更新理发任务完成计数器
        pthread_mutex_lock(&taskCounterMutex);
        barberTasksCompleted++;
        pthread_mutex_unlock(&taskCounterMutex);
    }
    return NULL;
}

// 顾客线程
void* customer_thread(void* arg) {
    int customerId = *(int*)arg;

    pthread_mutex_lock(&mutex);

    if (seatCount < N) {
        // 有空座位
        seatQueue[seatCount++] = customerId;
        printf(YELLOW "[时钟 %ld ms] 顾客 %d 坐下等待理发。当前座位人数：%d\n" RESET, getCurrentTime(), customerId, seatCount);
        pthread_mutex_unlock(&mutex);

        sem_post(&barberReady);
        sem_wait(&availableSeats);
    } else {
        // 没有空座位，进入排队
        enqueue(customerId);
        printf(RED "[时钟 %ld ms] 顾客 %d 排队等待座位。当前排队人数：%d\n" RESET, getCurrentTime(), customerId, queueSize);
        pthread_mutex_unlock(&mutex);

        while (1) {
            pthread_mutex_lock(&mutex);
            if (seatCount < N && queueSize > 0 && waitingQueue[queueFront] == customerId) {
                dequeue();
                seatQueue[seatCount++] = customerId;
                printf(CYAN "[时钟 %ld ms] 顾客 %d 从排队区进入座位等待理发。当前座位人数：%d\n" RESET, getCurrentTime(), customerId, seatCount);
                pthread_mutex_unlock(&mutex);

                sem_post(&barberReady);
                sem_wait(&availableSeats);
                break;
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    return NULL;
}

// 检查输入是否为正整数
int isPositiveInteger(const char* str) {
    if (*str == '\0') return 0;
    
    while (*str) {
        if (!isdigit(*str)) {
            return 0; // 非数字字符
        }
        str++;
    }
    return 1; // 全部为数字
}

// 去除字符串中的换行符和空格
void trimNewline(char* str) {
    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == '\n' || *end == ' ')) {
        *end = '\0';
        end--;
    }
}

int main() {
    char buffer[100];
    
    // 获取理发师数量
    printf("请输入理发师数量: ");
    fgets(buffer, sizeof(buffer), stdin);
    trimNewline(buffer);
    if (!isPositiveInteger(buffer)) {
        printf(RED "输入无效！理发师数量必须是正整数。\n" RESET);
        return 1;
    }
    M = atoi(buffer);
    
    // 获取座位数量
    printf("请输入座位数量: ");
    fgets(buffer, sizeof(buffer), stdin);
    trimNewline(buffer);
    if (!isPositiveInteger(buffer)) {
        printf(RED "输入无效！座位数量必须是正整数。\n" RESET);
        return 1;
    }
    N = atoi(buffer);

    // 获取顾客数量
    printf("请输入顾客数量: ");
    fgets(buffer, sizeof(buffer), stdin);
    trimNewline(buffer);
    if (!isPositiveInteger(buffer)) {
        printf(RED "输入无效！顾客数量必须是正整数。\n" RESET);
        return 1;
    }
    CUSTOMER_COUNT = atoi(buffer);

    if (M <= 0 || N <= 0 || CUSTOMER_COUNT <= 0) {
        printf(RED "所有输入值必须大于零！\n" RESET);
        return 1;
    }

    pthread_t barbers[M], customers[CUSTOMER_COUNT];
    int barberIds[M], customerIds[CUSTOMER_COUNT];

    // 初始化队列和座位
    waitingQueue = (int*)malloc(CUSTOMER_COUNT * sizeof(int));
    seatQueue = (int*)malloc(N * sizeof(int));

    // 初始化同步机制
    sem_init(&availableSeats, 0, N);
    sem_init(&barberReady, 0, 0);
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&taskCounterMutex, NULL);

    // 获取程序启动时间
    gettimeofday(&startTime, NULL);

    printf(YELLOW "======== 理发店模拟程序开始 ========\n" RESET);

    // 创建理发师线程
    for (int i = 0; i < M; i++) {
        barberIds[i] = i + 1;
        pthread_create(&barbers[i], NULL, barber_thread, &barberIds[i]);
    }

    // 创建顾客线程
    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        customerIds[i] = i + 1;
        pthread_create(&customers[i], NULL, customer_thread, &customerIds[i]);
        delay(rand() % 2000 + 500); // 随机到达时间
    }

    // 等待所有顾客线程完成
    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        pthread_join(customers[i], NULL);
    }

    // 等待所有理发师完成任务
    while (1) {
        pthread_mutex_lock(&taskCounterMutex);
        if (barberTasksCompleted == CUSTOMER_COUNT) {
            pthread_mutex_unlock(&taskCounterMutex);
            break;
        }
        pthread_mutex_unlock(&taskCounterMutex);
    }

    printf(YELLOW "[时钟 %ld ms] 所有顾客完成理发。\n" RESET, getCurrentTime());
    printf(YELLOW "======== 理发店模拟程序结束 ========\n" RESET);

    // 清理资源
    free(waitingQueue);
    free(seatQueue);
    sem_destroy(&availableSeats);
    sem_destroy(&barberReady);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&taskCounterMutex);

    return 0;
}