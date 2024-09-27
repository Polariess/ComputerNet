#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket;  // 全局客户端socket
int running = 1;    // 用于控制客户端的运行状态
char input_buffer[BUFFER_SIZE];  // 用于保存用户输入的临时缓冲区
int input_length = 0;  // 当前输入长度
char user_name[BUFFER_SIZE]; //用于保存用户名 
char send_buffer[2*BUFFER_SIZE + 3]; // 用于存储最终发送的消息

// 处理服务器发送的消息
void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while (running) {
        n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n > 0) {
            buffer[n] = '\0';

            // 清除当前输入行
            printf("\r\033[K");  // 回到行首并清除该行
            printf("%s\n", buffer);  // 打印服务器消息

            // 重新显示输入提示符和输入的内容
            printf("Enter message: %s", input_buffer);
            fflush(stdout);  // 确保立即刷新输出
        }
		else if (n == 0) {
            printf("\nServer disconnected.\n");
            running = 0;
            break;
        }
		else {	
            perror("recv failed");
            running = 0;
            break;
        }
    }

    return NULL;
}

// 设置终端为非规范模式
void set_non_canonical_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);  // 关闭规范模式和回显
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// 恢复终端模式
void reset_terminal_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);  // 恢复规范模式和回显
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// 检查是否有输入可读
int kbhit() {
    struct termios oldt, newt;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int main() {
	//输入用户名 
	printf("Please Enter Your Name:");
	fgets(user_name, sizeof(user_name), stdin);
	user_name[strlen(user_name) - 1] = '\0'; // 删除换行符 
	
	
    struct sockaddr_in server_addr;
    pthread_t receive_thread;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("192.168.86.128");

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to server failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");
	printf("Your name: %s\n", user_name);
	printf("\n================\n\n");
	
    // 设置终端为非规范模式
    set_non_canonical_mode();

    // 创建接收消息的线程
    pthread_create(&receive_thread, NULL, receive_handler, NULL);

    // 主线程用于发送消息
    while (running) {
        // 显示输入提示符
        printf("Enter message: ");
        fflush(stdout);  // 刷新提示符，确保及时显示

        // 逐字符读取输入
        char ch;
        while (input_length < BUFFER_SIZE - 1 && running) {
            // 检查是否有输入可读
            if (kbhit()) {
                ch = getchar();
                if (ch == '\n') {
                	if (input_length > 0)
					{
						printf("\r\033[K");  // 回到行首并清除该行：包括enter message 
                    	break;  // 输入结束
					}
                }
				else if (ch == 127 || ch == '\b') { // 处理退格键
                    if (input_length > 0)
					{
                        input_length--;
                        input_buffer[input_length] = '\0';  // 清空最后一个字符
                        printf("\b \b");  // 删除最后一个字符
                    }
                }
				else {
                    input_buffer[input_length++] = ch;
                    input_buffer[input_length] = '\0';  // 确保字符串结束
                    printf("%c", ch);  // 显示当前输入字符
                    fflush(stdout);  // 确保立即显示
                }
            }
        }
        
        //检测是输入的待发送信息还是断开请求 
        if (strcmp(input_buffer, "quit") == 0) {
            printf("Client requested to quit.\n");
            running = 0;
            break;
        }
        
        // 发送消息
        if (input_length > 0) {
        	//连接字符串信息 
            snprintf(send_buffer, sizeof(send_buffer), "%s: %s", user_name, input_buffer);
		    if (send(client_socket, send_buffer, strlen(send_buffer), 0) == -1) {
		        perror("send failed");
		        break;
		    }
        }
        // 清空输入缓冲区
        input_length = 0;  // 重置输入长度
        memset(input_buffer, 0, sizeof(input_buffer));  // 清空输入内容
    } 
    // 恢复终端模式并关闭客户端socket
    reset_terminal_mode();
    close(client_socket);
    pthread_join(receive_thread, NULL);

    return 0;
}
