#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <gtk/gtk.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket;  // 全局客户端socket
struct sockaddr_in server_addr; //全局服务器端sockaddr 
int running = 1;    // 用于控制客户端的运行状态
char input_buffer[BUFFER_SIZE];  // 用于保存用户输入的临时缓冲区
int input_length = 0;  // 当前输入长度
char user_name[BUFFER_SIZE]; //用于保存用户名 
char send_buffer[3*BUFFER_SIZE + 30]; // 用于存储最终发送的消息

// 处理服务器发送的消息
void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    ssize_t n;
    while (running) {
        n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n > 0) {
        	if(running) {
        		buffer[n] = '\0';
                printf("%s\n", buffer);  // 打印服务器消息
			}
        }
		else if (n == 0) {
            printf("Server disconnected.\n");
            running = 0;
            break;
        }
		else {	
            perror("recv failed");
            running = 0;
            break;
        }
    }
	gtk_main_quit();
	//用来处理服务器端主动关闭连接的情况，这个代码在客户端发起关闭请求时不会有效果 
    return NULL;
}

//对输入的处理 
void message_enter(GtkWidget *widget, gpointer data) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
    
    // 获取文本缓冲区中的文本
    GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gchar *input_buffer = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    //检测是输入的待发送信息还是断开请求 
    if (strcmp(input_buffer, "quit") == 0) {
        if (send(client_socket, input_buffer, strlen(input_buffer), 0) == -1) {
		    perror("send failed");
		    close(client_socket);
		    exit(0);
		    //客户端发不过去，直接全关了 
		}
        printf("Client requested to quit.\n");
		//结束接收线程
        running = 0;
        // 销毁GTK应用程序
    	gtk_main_quit();
    }
    input_length = strlen(input_buffer);
	if (input_length > 0) {
		// 获取当前时间
		time_t raw_time;
		struct tm *time_info;
		char time_str[20];  // 用于存储时间戳的字符串
		time(&raw_time);  // 获取当前时间
		time_info = localtime(&raw_time);  // 将时间转换为本地时间
		strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);  // 格式化时间
			
		// 拼接消息，格式为：用户名 时间戳 换行 实际消息
		snprintf(send_buffer, sizeof(send_buffer), "%s %s\n%s", user_name, time_str, input_buffer);
		// 发送消息
		if (send(client_socket, send_buffer, strlen(send_buffer), 0) == -1) {
		    perror("send failed");
		    close(client_socket);
		    exit(0);
		    //客户端发不过去，直接全关了
		}
	}
    //清空文本框 
    gtk_text_buffer_set_text(buffer, "", -1);
}
int main() {
	//输入用户名 
	printf("Please Enter Your Name:");
	fgets(user_name, sizeof(user_name), stdin);
	user_name[strlen(user_name) - 1] = '\0'; // 删除换行符 
	
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
	printf("Your name: %s, please enter your message in the text viwer\n", user_name);
	printf("If you want to quit, Enter 'quit' and submit.\n"); 
	printf("\n================\n\n");
	
    // 创建接收消息的线程
    pthread_create(&receive_thread, NULL, receive_handler, NULL);

    // 主线程用于发送消息：其实是借助gtk_main线程
	gtk_init(NULL, NULL);

    // 创建窗口和布局
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    GtkWidget *layout = gtk_layout_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), layout);

    // 创建TextView控件
    GtkWidget *text_view = gtk_text_view_new();
    gtk_layout_put(GTK_LAYOUT(layout), text_view, 20, 20);
    gtk_widget_set_size_request(text_view, 400, 200);

    // 获取TextView的缓冲区
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    // 创建按钮，用于触发消息发送 
    GtkWidget *button = gtk_button_new_with_label("Enter message");
    gtk_layout_put(GTK_LAYOUT(layout), button, 20, 240);
    g_signal_connect(button, "clicked", G_CALLBACK(message_enter), buffer);

    gtk_widget_show_all(window);
    
    //开启输入处理线程 
    gtk_main(); 
    
    //输入处理线程结束，关闭客户端 
    close(client_socket);
    pthread_join(receive_thread, NULL);

    return 0;
}
