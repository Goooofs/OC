#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

#define MAX_CLIENTS 1

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) {
    wasSigHup = 1;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addrs[MAX_CLIENTS];
    socklen_t server_addr_len = sizeof(server_addr);
    int clients[MAX_CLIENTS] = {0};  
    int PORT = atoi(argv[1]);

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Привязка сокета к адресу и порту
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Прослушивание порта для входящих соединений
    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    // Регистрация обработчика сигнала
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // Блокировка сигнала
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask error");
        return 1;
    }

    printf("Server listening on port %d\n", PORT);
    struct timespec timeout = {5, 0}; 

    while (wasSigHup) {
        fd_set fds; //Создается набор файловых дескрипторов fds для использования в pselect.
        FD_ZERO(&fds); // Очищается набор fds и добавляется серверный сокет server_fd для отслеживания активности.
        FD_SET(server_fd, &fds);
        int maxFd = server_fd; //Инициализация переменной maxFd значением серверного сокета. Она используется для определения максимального значения файлового дескриптора в функции pselect.

        for (int i = 0; i < MAX_CLIENTS; i++) { //Этот цикл выполняет проход по массиву клиентских сокетов для добавления их в множество файловых дескрипторов fds.
            if (clients[i] > 0) { //Если clients[i] больше 0, это означает, что в этом слоте массива есть открытый сокет.
                FD_SET(clients[i], &fds); //Функция FD_SET добавляет клиентский сокет clients[i] в множество fds. Это необходимо для того, чтобы функция pselect следила за активностью на этом сокете.
                if (clients[i] > maxFd) { //Здесь проверяется, если текущий клиентский сокет clients[i] имеет больший номер дескриптора, чем текущее значение maxFd. Если это так, то maxFd обновляется значением клиентского сокета. 
                    maxFd = clients[i]; // то maxFd обновляется значением клиентского сокета.Это нужно для определения максимального значения файлового дескриптора, которое будет использовано в функции pselect. 
                }   //Функция pselect ожидает события для файловых дескрипторов от 0 до maxFd, включительно.
            }
        }

        int pselect_flag = pselect(maxFd + 1, &fds, NULL, NULL, &timeout, &origMask);

        if (pselect_flag == -1) {
            if (errno == EINTR){
                if (wasSigHup) {
                    printf("Received SIGHUP signal\n");
                    wasSigHup = 0;
                }
            }else {
                perror("pselect error");
                break;
            } 
        } else if (pselect_flag == 0) {
            printf("No activity on sockets\n");
        } else {
            // Обработка произошедшего события
            if (FD_ISSET(server_fd, &fds)) {
                int new_client_index = -1; //Это переменная, которая будет использоваться для хранения индекса свободного слота в массиве clients, куда будет добавлен новый клиент.
                for (int i = 0; i < MAX_CLIENTS; i++) { // Это цикл, который итерирует по массиву clients, ища первый свободный слот (где clients[i] == 0).
                    if (clients[i] == 0) { //Если найден свободный слот, индекс этого слота сохраняется в new_client_index, и цикл завершается с помощью break.
                        new_client_index = i;
                        break;
                    }
                }

                if (new_client_index != -1) { //После завершения цикла проверяется, был ли найден свободный слот. Если new_client_index не равен -1, это означает, что был найден свободный слот.
                    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addrs[new_client_index], &server_addr_len)) == -1) {
                        perror("Accept failed"); //В этой строке кода выполняется accept, чтобы принять новое подключение. Если accept завершается с ошибкой, программа выводит сообщение об ошибке и возвращает код ошибки.
                        return 1;
                    } else {
                        char client_ip[INET_ADDRSTRLEN];//Создается буфер для хранения IP-адреса клиента.
                        inet_ntop(AF_INET, &client_addrs[new_client_index].sin_addr, client_ip, sizeof(client_ip));//Преобразует IP-адрес из структуры sockaddr_in в строку и сохраняет его в client_ip.
                        int client_port = ntohs(client_addrs[new_client_index].sin_port);//Получает порт клиента из структуры sockaddr_in.
                        //Если accept прошел успешно, выполняется блок кода, который сохраняет информацию о новом клиенте и выводит сообщение об успешном подключении на терминал.
                        clients[new_client_index] = client_fd;//Сохраняет файловый дескриптор нового клиента в массиве clients.
                        printf("New connection accepted from %s:%d\n", client_ip, client_port);
                    }
                } else {
                    // Если все слоты заняты, отклоняем новое подключение
                    struct sockaddr_in temp_client_addr;//Создается временная структура для хранения информации о клиенте.
                    socklen_t temp_client_addr_len = sizeof(temp_client_addr);//Получает размер структуры temp_client_addr.
                    int temp_client_fd = accept(server_fd, (struct sockaddr *)&temp_client_addr, &temp_client_addr_len);//Принимает новое подключение для временного сокета.
                    if (temp_client_fd != -1) {// Если accept завершился успешно, то это новое подключение будет отклонено.
                        char temp_client_ip[INET_ADDRSTRLEN];//Создается буфер для хранения IP-адреса клиента.
                        inet_ntop(AF_INET, &temp_client_addr.sin_addr, temp_client_ip, sizeof(temp_client_ip));//Преобразует IP-адрес из структуры sockaddr_in в строку и сохраняет его в temp_client_ip.
                        int temp_client_port = ntohs(temp_client_addr.sin_port);//Получает порт клиента из структуры sockaddr_in.

                        printf("Connection from %s:%d rejected. Maximum clients reached.\n", temp_client_ip, temp_client_port);
                        //Выводит сообщение на терминал о том, что новое подключение было отклонено из-за достижения максимального числа клиентов.
                        close(temp_client_fd);// Закрывает временный сокет.
                    }
                }
            }

            // Проверка активности каждого соединения
            for (int i = 0; i < MAX_CLIENTS; i++) {//Это цикл, который проходит по массиву clients, содержащему файловые дескрипторы соединений с клиентами.
                if (clients[i] > 0 && FD_ISSET(clients[i], &fds)) {//Эта проверка осуществляется для каждого элемента массива clients. Она проверяет, является ли файловый дескриптор положительным числом 
                char buffer[1024] = {0};                   //(что означает, что соединение с клиентом открыто), и был ли файловый дескриптор установлен в наборе fds (что означает, что на соединении есть активность).
                ssize_t valread = read(clients[i], buffer, sizeof(buffer));
                //Выполняется чтение данных от клиента в буфер buffer. Функция read возвращает количество прочитанных байтов, и значение сохраняется в valread.
                char client_ip[INET_ADDRSTRLEN];//Создается буфер для хранения IP-адреса клиента.
                inet_ntop(AF_INET, &client_addrs[i].sin_addr, client_ip, sizeof(client_ip));//Преобразует IP-адрес из структуры sockaddr_in в строку и сохраняет его в client_ip.
                int client_port = ntohs(client_addrs[i].sin_port);//Получает порт клиента из структуры sockaddr_in.

                    if (valread == -1) {
                        perror("Recv failed");
                    } else if (valread == 0) {
                        printf("Connection closed by client %s:%d\n", client_ip, client_port);
                        close(clients[i]);
                        clients[i] = 0;
                    } else if (valread > 0) {
                        printf("Received data from client %s:%d: %s", client_ip, client_port, buffer);
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] > 0) {
            close(clients[i]);
        }
    }

    close(server_fd);

    return 0;
}