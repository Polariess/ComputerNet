#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

int clients[MAX_CLIENTS];
int client_count = 0;
int server_running = 1;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(const char *message, int sender_socket) {
    int client_sockets[MAX_CLIENTS];  // 临时存储客户端socket
    int current_client_count;

    // 加锁仅复制客户端列表，缩短持锁时间
    pthread_mutex_lock(&clients_mutex);
    current_client_count = client_count;
    for (int i = 0; i < current_client_count; i++) {
        client_sockets[i] = clients[i];
    }
    pthread_mutex_unlock(&clients_mutex);

    // 解锁后广播消息
	for (int i = 0; i < current_client_count; i++) {
	    char send_message[BUFFER_SIZE];  // 临时字符串
	    if (client_sockets[i] != sender_socket) {
	        // 对其他客户端发送消息
	        send(client_sockets[i], message, strlen(message), 0);
	    }
		else {
	        // 对发送者自己发送消息，添加 "(You)"
	        snprintf(send_message, sizeof(send_message), "(You) %s", message);
	        send(client_sockets[i], send_message, strlen(send_message), 0);
	    }
	}
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while ((n = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0 && server_running) {
        buffer[n] = '\0';
        printf("Received from client %d: %s\n", client_socket, buffer);

        // 广播信息到除发送者之外的所有客户端 
        broadcast_message(buffer, client_socket);

        // 检查退出信息 
        if (strcmp(buffer, "quit") == 0) {
            printf("Client %d requested to quit.\n", client_socket);
            break;
        }
    }
    
    // 准备挪出客户端 
    pthread_mutex_lock(&clients_mutex);
    
    //关闭连接由server_running触发，提示服务器端关闭信息 
    if(!server_running) {
		send(client_socket, "Server is shutting down.\n", 25, 0);
	}
    
    //防止在关闭服务器端中关闭后再关闭一次 
    if(client_count > 0) {
		close(client_socket);
	}
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i] == client_socket) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    printf("Client %d disconnected.\n", client_socket);
    return NULL;
}

void *command_interface(void *arg) {
    char command[BUFFER_SIZE];
    while (server_running) {
        printf("If you want to shut down, enter 'quit'\n");
        fgets(command, sizeof(command), stdin);

        if (strncmp(command, "quit", 4) == 0) {
            printf("Shutting down server...\n");
            server_running = 0;
            //关闭服务器：设server_running = 0 
			
			//新建虚拟client，向server发起虚假的连接请求，将主线程阻塞过程跳过
			struct sockaddr_in server_addr;
			server_addr.sin_family = AF_INET;
    		server_addr.sin_port = htons(PORT);
    		server_addr.sin_addr.s_addr = inet_addr("192.168.86.128");
    		
    		int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    		
    		if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        		perror("Connection to server failed");
        		close(client_socket);
        		exit(EXIT_FAILURE);
    		}
			
			//建立虚拟连接后什么都不需要做，让主线程自己去创建新线程、挪 
		    
            break;
        } 
		else {
            printf("Unknown command: %s", command);
        }
    }
    return NULL;
}

void shutdown_server(int server_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        send(clients[i], "Server is shutting down.\n", 25, 0);
        close(clients[i]);
    }
    client_count = 0;
    pthread_mutex_unlock(&clients_mutex);
    close(server_socket);
    printf("Server shutdown complete.\n");
}

int main() {
    int server_socket, *new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t tid, cmd_tid;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // 启动命令行界面线程
    pthread_create(&cmd_tid, NULL, command_interface, NULL);

    while (server_running) {
        addr_size = sizeof(client_addr);
        new_socket = malloc(sizeof(int));
        *new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (*new_socket == -1) {
            if (server_running) {
                perror("Accept failed");
            }
            free(new_socket);
            continue;
        }
		
        // Add new client to the list
        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            clients[client_count++] = *new_socket;
            printf("New connection accepted: client %d\n", *new_socket);
            pthread_create(&tid, NULL, handle_client, new_socket);
            pthread_detach(tid);
        }
		else {
            printf("Max clients reached. Connection rejected: client %d\n", *new_socket);
            close(*new_socket);
            free(new_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // 等待命令行线程结束
    pthread_join(cmd_tid, NULL);

    // 关闭服务器
    shutdown_server(server_socket);

    return 0;
}

