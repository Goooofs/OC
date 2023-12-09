#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex;    // Мьютекс для синхронизации потоков
pthread_cond_t condition;  // Условная переменная для сигнализации о событии

int eventFlag = 0;        // Флаг события

// Поток-поставщик
void* producer(void* arg) {
    while (1) {
        sleep(1);  // Задержка в одну секунду
        pthread_mutex_lock(&mutex);  // Захватываем мьютекс

        if(eventFlag == 1){
            pthread_mutex_unlock(&mutex);   // Освобождаем мьютекс
            printf("Поставщик: событие не обработанно");
            continue;    // Переходим к следующей итерации
        }
        
        eventFlag = 1; // Устанавливаем флаг события
        printf("Поставщик: отправлено событие\n");

        pthread_cond_signal(&condition); // Сообщаем потребителю о событии
        pthread_mutex_unlock(&mutex);   // Освобождаем мьютекс
    }
    return NULL;
}

// Поток-потребитель
void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);  // Захватываем мьютекс

        while (eventFlag == 0) {
            pthread_cond_wait(&condition, &mutex); // Ожидаем события и освобождаем мьютекс
        }

        eventFlag = 0; // Сбрасываем флаг события
        printf("Потребитель: получено событие\n\n");

        pthread_mutex_unlock(&mutex);  // Освобождаем мьютекс
    }
    return NULL;
}

int main() {
    pthread_t producer_thread, consumer_thread;

    pthread_mutex_init(&mutex, NULL);   // Инициализируем мьютекс
    pthread_cond_init(&condition, NULL); // Инициализируем условную переменную

    pthread_create(&producer_thread, NULL, producer, NULL); // Создаем поток-поставщик
    pthread_create(&consumer_thread, NULL, consumer, NULL); // Создаем поток-потребитель

    pthread_join(producer_thread, NULL); // Ждем завершения потока-поставщика
    pthread_join(consumer_thread, NULL); // Ждем завершения потока-потребителя

    pthread_mutex_destroy(&mutex);   // Уничтожаем мьютекс
    pthread_cond_destroy(&condition); // Уничтожаем условную переменную

    return 0;
}
