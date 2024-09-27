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

int client_socket;  // ȫ�ֿͻ���socket
int running = 1;    // ���ڿ��ƿͻ��˵�����״̬
char input_buffer[BUFFER_SIZE];  // ���ڱ����û��������ʱ������
int input_length = 0;  // ��ǰ���볤��
char user_name[BUFFER_SIZE]; //���ڱ����û��� 
char send_buffer[2*BUFFER_SIZE + 3]; // ���ڴ洢���շ��͵���Ϣ

// ������������͵���Ϣ
void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while (running) {
        n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n > 0) {
            buffer[n] = '\0';

            // �����ǰ������
            printf("\r\033[K");  // �ص����ײ��������
            printf("%s\n", buffer);  // ��ӡ��������Ϣ

            // ������ʾ������ʾ�������������
            printf("Enter message: %s", input_buffer);
            fflush(stdout);  // ȷ������ˢ�����
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

// �����ն�Ϊ�ǹ淶ģʽ
void set_non_canonical_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);  // �رչ淶ģʽ�ͻ���
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// �ָ��ն�ģʽ
void reset_terminal_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);  // �ָ��淶ģʽ�ͻ���
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// ����Ƿ�������ɶ�
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
	//�����û��� 
	printf("Please Enter Your Name:");
	fgets(user_name, sizeof(user_name), stdin);
	user_name[strlen(user_name) - 1] = '\0'; // ɾ�����з� 
	
	
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
	
    // �����ն�Ϊ�ǹ淶ģʽ
    set_non_canonical_mode();

    // ����������Ϣ���߳�
    pthread_create(&receive_thread, NULL, receive_handler, NULL);

    // ���߳����ڷ�����Ϣ
    while (running) {
        // ��ʾ������ʾ��
        printf("Enter message: ");
        fflush(stdout);  // ˢ����ʾ����ȷ����ʱ��ʾ

        // ���ַ���ȡ����
        char ch;
        while (input_length < BUFFER_SIZE - 1 && running) {
            // ����Ƿ�������ɶ�
            if (kbhit()) {
                ch = getchar();
                if (ch == '\n') {
                	if (input_length > 0)
					{
						printf("\r\033[K");  // �ص����ײ�������У�����enter message 
                    	break;  // �������
					}
                }
				else if (ch == 127 || ch == '\b') { // �����˸��
                    if (input_length > 0)
					{
                        input_length--;
                        input_buffer[input_length] = '\0';  // ������һ���ַ�
                        printf("\b \b");  // ɾ�����һ���ַ�
                    }
                }
				else {
                    input_buffer[input_length++] = ch;
                    input_buffer[input_length] = '\0';  // ȷ���ַ�������
                    printf("%c", ch);  // ��ʾ��ǰ�����ַ�
                    fflush(stdout);  // ȷ��������ʾ
                }
            }
        }
        
        //���������Ĵ�������Ϣ���ǶϿ����� 
        if (strcmp(input_buffer, "quit") == 0) {
            printf("Client requested to quit.\n");
            running = 0;
            break;
        }
        
        // ������Ϣ
        if (input_length > 0) {
        	//�����ַ�����Ϣ 
            snprintf(send_buffer, sizeof(send_buffer), "%s: %s", user_name, input_buffer);
		    if (send(client_socket, send_buffer, strlen(send_buffer), 0) == -1) {
		        perror("send failed");
		        break;
		    }
        }
        // ������뻺����
        input_length = 0;  // �������볤��
        memset(input_buffer, 0, sizeof(input_buffer));  // �����������
    } 
    // �ָ��ն�ģʽ���رտͻ���socket
    reset_terminal_mode();
    close(client_socket);
    pthread_join(receive_thread, NULL);

    return 0;
}
