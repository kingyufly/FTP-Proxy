#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#define BUFFSIZE 1023

#define ftpPort 21
#define proxyAddr "192.168.56.101"
#define serverAddr "192.168.56.1"

int get_free_port();

int main(int argc, const char *argv[])
{
    fd_set master_set, working_set;  //文件描述符集合
    struct timeval timeout;          //select 参数中的超时结构体
    int proxy_cmd_socket    = 0;     //proxy listen控制连接
    int accept_cmd_socket   = 0;     //proxy accept客户端请求的控制连接
    int connect_cmd_socket  = 0;     //proxy connect服务器建立控制连接
    int proxy_data_socket   = 0;     //proxy listen数据连接
    int accept_data_socket  = 0;     //proxy accept得到请求的数据连接（主动模式时accept得到服务器数据连接的请求，被动模式时accept得到客户端数据连接的请求）
    int connect_data_socket = 0;     //proxy connect建立数据连接 （主动模式时connect客户端建立数据连接，被动模式时connect服务器端建立数据连接）
    int selectResult = 0;     //select函数返回值
    int select_sd = 10;    //select 函数监听的最大文件描述符
    
    int PORT_flag = -1;     //0 for standard mode; 1 for passive mode
    int RETR_flag = -1;     //0 for download(RETR); 1 for upload(STOR)
    int cache_flag = -1;      //0 for no such file in the cache; 1 for there is such file in the cache  
    int type_flag = -1;      //0 for not pdf,jpg; 1 for is pdf,jpg   
    int freePort = -1, freePortA = -1, freePortB = -1;
    int connect1 = -1;

    unsigned short int clientPort; // 用来储存主动模式时的port数 
    unsigned short int serverPort; // 用来储存被动模式时的pasv数（返回的端口号） 
    
    FD_ZERO(&master_set);   //清空master_set集合
    bzero(&timeout, sizeof(timeout));
    
    char clientAddr[15]; // port模式时，储存clientIP address  
    char cachePath[BUFFSIZE]; // cache 路径 
    char retrbuff[BUFFSIZE]; // passive 模式时RETR命令的存储，用于传递 
    char file[BUFFSIZE]; // RETR 的文件名 
    char type[50]; // RETR 的文件类型 
    int i = 0;
    
    int value = 1; //setsockopt时使用原为Boolean值true（enable） 
    int length = 0; // buffer 读出的长度 

    int cache_fd;  //查看cache的文件标识符 
    
    //proxy_cmd_socket = bindAndListenSocket();  //开启proxy_cmd_socket、bind（）、listen操作
    struct sockaddr_in proxy_cmd_struct;
    if ((proxy_cmd_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
      printf("socket() failed.\n");
      exit(1);
    }

    memset(&proxy_cmd_struct, 0, sizeof(proxy_cmd_struct));
    proxy_cmd_struct.sin_family = AF_INET;
    proxy_cmd_struct.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy_cmd_struct.sin_port = htons(ftpPort);

    if ((bind(proxy_cmd_socket, (struct sockaddr *) &proxy_cmd_struct, sizeof(proxy_cmd_struct))) < 0){
      printf("bind1() failed.\n");
      close(proxy_cmd_socket);
      exit(1);
    }
    
    if(listen(proxy_cmd_socket,2) == -1){
      printf("listen() failed.\n");
      close(proxy_cmd_socket);
      exit(1);
    } 

    FD_SET(proxy_cmd_socket, &master_set);  //将proxy_cmd_socket加入master_set集合
    
    timeout.tv_sec = NULL;    //Select的超时结束时间
    timeout.tv_usec = NULL;    //ms
    
    while (1) {
        FD_ZERO(&working_set); //清空working_set文件描述符集合
        memcpy(&working_set, &master_set, sizeof(master_set)); //将master_set集合copy到working_set集合
        
        //select循环监听 这里只对读操作的变化进行监听（working_set为监视读操作描述符所建立的集合）,第三和第四个参数的NULL代表不对写操作、和误操作进行监听
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);

        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout
        //if (selectResult == 0) {
        //    printf("select() timed out.\n");
        //    continue;
        //}
        // selectResult > 0 时 开启循环判断有变化的文件描述符为哪个socket
        for (i = 0; i < select_sd; i++) {
            //判断变化的文件描述符是否存在于working_set集合
            if (FD_ISSET(i, &working_set)) {
                // 建立控制链接 client->proxy->server 
                if (i == proxy_cmd_socket) { 
                    //accept_cmd_socket = acceptCmdSocket();  //执行accept操作,建立proxy和客户端之间的控制连接
                    struct sockaddr_in accept_cmd_struct; /* Client address */
                    unsigned int acceptStructLen; /* Length of client address */
                    memset(&accept_cmd_struct, 0, sizeof(accept_cmd_struct));
                    accept_cmd_struct.sin_family = AF_INET;
                    acceptStructLen = sizeof(accept_cmd_struct);

                    accept_cmd_socket = accept(proxy_cmd_socket, (struct sockaddr *)&accept_cmd_struct, &acceptStructLen); 
                    
                    if(accept_cmd_socket == -1){
                        printf("connect() failed.\n");
	                    close(accept_cmd_socket);
	                    exit(1);  
                    } 
                    
                    //connect_cmd_socket = connectToServer(); //执行connect操作,建立proxy和服务器端之间的控制连接       
                    if ((connect_cmd_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                        printf("socket() failed.\n");
    	                exit(1);
                    }
                    
                    struct sockaddr_in connect_cmd_struct;   
                    memset(&connect_cmd_struct, 0, sizeof(connect_cmd_struct));
                    connect_cmd_struct.sin_family = AF_INET;
                    connect_cmd_struct.sin_addr.s_addr = inet_addr(serverAddr);
                    connect_cmd_struct.sin_port = htons(ftpPort); 
        
                    if(connect(connect_cmd_socket,(struct sockaddr *)&connect_cmd_struct,sizeof(connect_cmd_struct)) == -1){
    	                printf("connect() failed.\n");
    	                close(connect_cmd_socket);
    	                exit(1);
                    }
                        
                    //将新得到的socket加入到master_set结合中
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }   
                //client传来command的处理    
                if (i == accept_cmd_socket) {
                    printf("accept_cmd_socket\n");
                    char buff[BUFFSIZE] = {0};
                       
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(connect_cmd_socket);
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);
                    } else {
                        printf("accept_cmd_socket: %s\n",buff);
                        buff[length] = '\0';
                        //如果接收到内容,则对内容进行必要的处理，之后发送给服务器端（写入connect_cmd_socket）
                        
                        //port 命令， IP address 提取， port提取 
                        if(strstr(buff,"PORT") != NULL){
                            printf("%s\n",buff);
                            PORT_flag = 1;
                            char* portchar = strtok(buff, " ");
                            char* ip1 = strtok(NULL, ",");
                            char* ip2 = strtok(NULL, ",");
                            char* ip3 = strtok(NULL, ",");
                            char* ip4 = strtok(NULL, ",");
                            char* portA = strtok(NULL, ",");
                            char* portB = strtok(NULL, "\r\n");
    
                            sprintf(clientAddr,"%s.%s.%s.%s",ip1,ip2,ip3,ip4); //获得clientIP address 
                            clientPort = atoi(portA)*256 + atoi(portB);
                     
                            freePort = get_free_port(); //选取可用端口 
                            freePortA = freePort/256;
                            freePortB = freePort%256;
    
                            //因为client和proxy为同一IP，所以直接用clientIP，应该改为proxy的固定IP 
                            sprintf(buff, "PORT %d,%d,%d,%d,%d,%d\r\n",192,168,56,101,freePortA,freePortB);
                              
                            if(send(connect_cmd_socket, buff, sizeof(buff), 0) != sizeof(buff)){
                                printf("%s\n", "send() error!");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              
                            //监听端口
                            ///////////////////////////////////////
                            if ((proxy_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                                printf("185 socket() failed.\n");
                                exit(1);
                            }
                            
                            struct sockaddr_in proxy_data_struct; /* Local address */
                            memset(&proxy_data_struct, 0, sizeof(proxy_data_struct));
                            proxy_data_struct.sin_family = AF_INET;
                            proxy_data_struct.sin_addr.s_addr = htonl(INADDR_ANY);
                            proxy_data_struct.sin_port = htons(freePort);
    
                            //setsockopt (proxy_data_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
                            if ((bind(proxy_data_socket, (struct sockaddr *) &proxy_data_struct, sizeof(proxy_data_struct))) < 0){
                                printf("195 bind() failed.\n");
                                close(proxy_data_socket);
                                exit(1);
                            }
        
                            if(listen(proxy_data_socket,2) == -1){
                                printf("201 listen() failed.\n");
                                close(proxy_data_socket);
                                exit(1);
                            }  
                            FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
                            printf("proxy_data_socket ok\n");
                        }    
                        // pasv命令监听 
                        else if(strstr(buff,"PASV") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("210 send() failed.\n");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                            PORT_flag = 0;
                        }
                        //RETR命令监听， 获取文件名，并进行判断是否有缓存，根据主动\被动模式进行执行 
                        else if(strstr(buff,"RETR ") != NULL){//用access代替
                            char* retrChar = strtok(buff, " ");
                            char* fileName = strtok(NULL, ".");
                            char* fileType = strtok(NULL, "\r\n");
                            
                            strcpy(file, fileName);
                            strcpy(type, fileType);
                            
                            if((strstr(type,"txt") != NULL) || (strstr(type,"jpg") != NULL)|| (strstr(type,"TXT") != NULL) || (strstr(type,"JPG") != NULL))
                                type_flag = 1;
                            else 
                                type_flag = -1;
                                
                            printf("filetype: %s   file_flag: %d\n",fileType, type_flag);
                            sprintf(buff, "%s %s.%s\r\n", "RETR", file, type);
                            sprintf(cachePath, "/root/cache/%s.%s", file, type);
		                    
		                    printf("buff: %s\n",buff);
		                    printf("cache path: %s\n",cachePath);

                            if((cache_fd = open(cachePath, O_RDONLY, 0)) == -1){  
            		            printf("no such file\n");
            		            cache_flag = 0;
            		            close(cache_fd);
            		            if(send(connect_cmd_socket, buff, length, 0) != length){
                                    printf("220 send() failed.\n");
                                    close(i); //如果发送错误,则关闭Socket
                                    close(connect_cmd_socket);
                                    //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                    FD_CLR(i, &master_set);
                                    FD_CLR(connect_cmd_socket, &master_set);
                                }
                                RETR_flag = 1;
                                
        	                }else if(PORT_flag == 1){ //port模式，不传RETR命令，模拟server与client进行数据传递 

                                printf("there is such file\n");
                                cache_flag = 1;   
                                /////////////////////////////////////////

                                RETR_flag = 1;
                                if ((connect_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                                    printf("356 socket() failed.\n");
    	                            exit(1);
                                }
                        
                                //设置socket选项，使得端口close后能够再次bind同一端口，即
                                //在主动模式时，proxy始终以（21-1）端口连接client 
                                setsockopt (connect_data_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
                                
                                struct sockaddr_in bind_struct;   
                                memset(&bind_struct, 0, sizeof(bind_struct));
                                bind_struct.sin_family = AF_INET;
                                bind_struct.sin_addr.s_addr = inet_addr("192.168.56.101");
                                bind_struct.sin_port = htons((ftpPort - 1)); 
                        
                                if ((bind(connect_data_socket, (struct sockaddr *) &bind_struct, sizeof(bind_struct))) < 0){
                                    printf("300 bind() failed.\n");
                                    close(connect_data_socket);
                                    exit(1);
                                }
                                
                                struct sockaddr_in connect_data_struct;   
                                memset(&connect_data_struct, 0, sizeof(connect_data_struct));
                                connect_data_struct.sin_family = AF_INET;
                                connect_data_struct.sin_addr.s_addr = inet_addr(clientAddr);
                                connect_data_struct.sin_port = htons(clientPort); 
        
        
                                char send_buff[BUFFSIZE] = {0};
                                
                                //返回文件前发送150 
                                memset(send_buff, 0, sizeof(send_buff));
                                sprintf(send_buff, "150 Opening data channel for file download from server of \"/%s.%s\"\r\n\0", file, type);
                                printf("%s\n",send_buff); 
                                printf("%d", strlen(send_buff));                          
                                if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(150) failed.\n");
                                }
                                
                                
                                if(connect(connect_data_socket,(struct sockaddr *)&connect_data_struct,sizeof(connect_data_struct)) == -1){
                                    printf("366 connect() failed.\n");
    	                            close(connect_data_socket);
    	                            exit(1);
                                }
                                
                                
                                //从cache中返回文件
                                memset(send_buff, 0, sizeof(send_buff));
                                printf("connect_data_socket ok!\n");
                                length = read (cache_fd, send_buff, 1023);  
                                send_buff[length]  = '\0';
                                
                                while(length > 0 && length == 1023){
                                    if(send(connect_data_socket,send_buff,1023,0) != 1023){
                                        printf("send(data) failed.\n");
                                    }
                                    length = read (cache_fd, send_buff, 1023);  
                                }
                                
                                if(send(connect_data_socket, send_buff, length, 0) != length){
                                    printf("send(data final) failed.\n");
                                }
                                
                                
                                //返回文件后发送226确认 
                                memset(send_buff, 0, sizeof(send_buff));
                                sprintf(send_buff, "226 Successfully transferred \"/%s.%s\"\r\n\0", file, type);
                                if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(226) failed.\n");
                                }
                                
                                close(cache_fd);
                                close(connect_data_socket);
                                close(proxy_data_socket);
                                FD_CLR(proxy_data_socket, &master_set);
                                
                                cache_flag = -1;
                                break;
                            } else if (PORT_flag == 0){
                                RETR_flag = 1;
                                cache_flag = 1;

                                printf("connect: %d", connect1);
                                
                                
                                if(cache_flag == 1 && connect1 == -1)
                                    connect1 = 1;
                                if(cache_flag == 1 && connect1 == 0){ //如果有缓存，则模拟server直接传送文件 
                                    printf("duankailianjie\n");
                                    //断开与server的数据连接         
                                    
                                    close(connect_data_socket);
                                    FD_CLR(connect_data_socket, &master_set);
                                
                                    char send_buff[BUFFSIZE] = {0};
                               
                                    memset(send_buff, 0, sizeof(send_buff));
                                    sprintf(send_buff, "150 Opening data channel for file download from server of \"/%s.%s\"\r\n", file, type);
                                    printf("%s\n",send_buff);                           
                                    if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(150) failed.\n");
                                    }
                                
                                    memset(send_buff, 0, sizeof(send_buff));
                                    printf("connect_data_socket ok!\n");
                                    length = read (cache_fd, send_buff, 1023);  
                                    send_buff[length]  = '\0';
                                
                                    while(length > 0 && length == 1023){
                                    if(send(accept_data_socket,send_buff,1023,0) != 1023){
                                        //printf("send(data)0 failed.\n");
                                    }
                                    length = read (cache_fd, send_buff, 1023);  
                                    }
                                
                                    if(send(accept_data_socket, send_buff, length, 0) != length){
                                    printf("send(data final) failed.\n");
                                    }
                                
                                    memset(send_buff, 0, sizeof(send_buff));
                                    sprintf(send_buff, "226 Successfully transferred \"/%s.%s\"\r\n", file, type);
                                    if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(226) failed.\n");
                                    }
                                    close(cache_fd);
                                
                                    close(proxy_data_socket);
                                    FD_CLR(proxy_data_socket, &master_set);
                                    close(accept_data_socket);
                                    FD_CLR(accept_data_socket, &master_set);
                                    cache_flag = -1;
                                    RETR_flag = -1; 
                                    connect1 = -1;
                                }
                                
                                printf("over\n");
                            }        
                        }
                        //上传文件时不判断直接转发 
                        else if(strstr(buff,"STOR") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("231 send() failed.\n");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              RETR_flag = 0;
                        }
                                
                        //写入proxy与server建立的cmd连接,除了PORT之外，直接转发buff内容
                        //write(connect_cmd_socket), buff, strlen(buff));
                        else{
                            printf("esle: %s\n",buff);
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("245 send() failed.\n");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }
                    }   
                }  
                if (i == connect_cmd_socket) { //server返回命令处理 
                    
                    char buff[BUFFSIZE] = {0};
                    
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        printf("close socket!\n");
                        close(i); //如果接收不到内容,则关闭Socket
                        close(accept_cmd_socket);     
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket, &master_set);
                    } else {
                        buff[length] = '\0';
                        printf("connect_cmd_socket:%s\n",buff);  
                        //处理服务器端发给proxy的reply，写入accept_cmd_socket
                        //passive mode 时返回227命令， 截取port，先监听，后改变ip和port发送至client 
                        if(strstr(buff,"227 Entering Passive Mode") != NULL){
                            PORT_flag = 0;
                            char* pasvchar = strtok(buff, "(");
                            char* ip1 = strtok(NULL, ",");
                            char* ip2 = strtok(NULL, ",");
                            char* ip3 = strtok(NULL, ",");
                            char* ip4 = strtok(NULL, ",");
                            char* portA = strtok(NULL, ",");
                            char* portB = strtok(NULL, "\r\n");
    
                            serverPort = atoi(portA)*256 + atoi(portB);                      
                            freePort = get_free_port();
                            freePortA = freePort/256;
                            freePortB = freePort%256;
                              
                            //因为client和proxy为同一IP，所以直接用clientIP，应该改为proxy的固定IP 
                            sprintf(buff, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",192,168,56,101,freePortA,freePortB);
    
                            //监听端口
                            ///////////////////////////////////////
                            struct sockaddr_in proxy_data_struct; /* Local address */
                            if ((proxy_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                                printf("290 socket() failed.\n");
                                exit(1);
                            }
    
                            memset(&proxy_data_struct, 0, sizeof(proxy_data_struct));
                            proxy_data_struct.sin_family = AF_INET;
                            proxy_data_struct.sin_addr.s_addr = htonl(INADDR_ANY);
                            proxy_data_struct.sin_port = htons(freePort);
    
                            if ((bind(proxy_data_socket, (struct sockaddr *) &proxy_data_struct, sizeof(proxy_data_struct))) < 0){
                                printf("300 bind() failed.\n");
                                close(proxy_data_socket);
                                exit(1);
                            }
        
                            if(listen(proxy_data_socket,2) == -1){
                                printf("306 listen() failed.\n");
                                close(proxy_data_socket);
                                exit(1);
                            } 
                            FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
                            
                            //先进行监听再发送pasv参数以防connection refused 
                            if(send(accept_cmd_socket, buff, strlen(buff), 0) != strlen(buff)){
                                printf("314 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                            printf("passive send ok!\n");
                        } else if(strstr(buff,"226 ABOR command successful") != NULL){
                            printf("jiedaole 226"); 
                        } 
                        //其他直接转发 
                        else{
                            if(send(accept_cmd_socket, buff, length, 0) != length){
                                printf("324 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_cmd_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }    
                    }
                }
                //监听端口，主动模式监听20端口，被动模式监听给定端口 
                if (i == proxy_data_socket) {
                    printf("proxy_data_socket\n");
                    //proxy accept得到请求的数据连接（主动模式时accept得到服务器数据连接的请求，被动模式时accept得到客户端数据连接的请求）
                    //proxy connect建立数据连接 （主动模式时connect客户端建立数据连接，被动模式时connect服务器端建立数据连接）
                    //主动模式，接收连接，并连接至server 
                    if(PORT_flag == 1){
                        //accept_data_socket
                        struct sockaddr_in accept_data_struct; /* Client address */
                        unsigned int acceptdataStructLen; /* Length of client address */
                        memset(&accept_data_struct, 0, sizeof(accept_data_struct));
                        accept_data_struct.sin_family = AF_INET;
                        acceptdataStructLen = sizeof(accept_data_struct);
    
                        accept_data_socket = accept(proxy_data_socket, (struct sockaddr *)&accept_data_struct, &acceptdataStructLen); 
                        if(accept_data_socket == -1){
                            printf("247 accept() failed.\n");
                            close(accept_data_socket);
    	                    exit(1);  
                        } 
                      
                        //connect_data_socket
                        if ((connect_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                            printf("356 socket() failed.\n");
    	                    exit(1);
                        }
                        
                        setsockopt (connect_data_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
                        ////////////////////////////////////////
                        struct sockaddr_in bind_struct;   
                        memset(&bind_struct, 0, sizeof(bind_struct));
                        bind_struct.sin_family = AF_INET;
                        bind_struct.sin_addr.s_addr = inet_addr("192.168.56.101");
                        bind_struct.sin_port = htons((ftpPort - 1)); 
                        
                        if ((bind(connect_data_socket, (struct sockaddr *) &bind_struct, sizeof(bind_struct))) < 0){
                            printf("300 bind() failed.\n");
                            close(connect_data_socket);
                            exit(1);
                        }
                        ///////////////////////////////////////
                        
                        struct sockaddr_in connect_data_struct;   
                        memset(&connect_data_struct, 0, sizeof(connect_data_struct));
                        connect_data_struct.sin_family = AF_INET;
                        connect_data_struct.sin_addr.s_addr = inet_addr(clientAddr);
                        connect_data_struct.sin_port = htons(clientPort); 
        
                        if(connect(connect_data_socket,(struct sockaddr *)&connect_data_struct,sizeof(connect_data_struct)) == -1){
                            printf("366 connect() failed.%d\n",PORT_flag);
    	                    close(connect_data_socket);
    	                    exit(1);
                        }
                        
                        //将新得到的socket加入到master_set结合中
                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                        //建立data连接(accept_data_socket、connect_data_socket)
                    } else if(PORT_flag == 0){ //被动模式，接收连接，如果本地有cache则不连接server（连接再断开） 
                        //accept_data_socket
                        printf("test1\n");
                        struct sockaddr_in accept_data_struct; /* Client address */
                        unsigned int acceptdataStructLen; /* Length of client address */
                        memset(&accept_data_struct, 0, sizeof(accept_data_struct));
                        accept_data_struct.sin_family = AF_INET;
                        acceptdataStructLen = sizeof(accept_data_struct);
    
                        accept_data_socket = accept(proxy_data_socket, (struct sockaddr *)&accept_data_struct, &acceptdataStructLen); 
                        if(accept_data_socket == -1){
                            printf("385 accept() failed.\n");
                            close(accept_data_socket);
    	                    exit(1);  
                        } 
                        
                        //connect_data_socket
                        struct sockaddr_in connect_data_struct;   
     
                        if ((connect_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                            printf("394 socket() failed.\n");
    	                    exit(1);
                        }
    
                        memset(&connect_data_struct, 0, sizeof(connect_data_struct));
                        connect_data_struct.sin_family = AF_INET;
                        connect_data_struct.sin_addr.s_addr = inet_addr(serverAddr);
                        connect_data_struct.sin_port = htons(serverPort); 
        
                        if(connect(connect_data_socket,(struct sockaddr *)&connect_data_struct,sizeof(connect_data_struct)) == -1){
                            printf("551 connect() failed.\n");
   	                        close(connect_data_socket);
   	                        exit(1);
                        }
                        FD_SET(connect_data_socket, &master_set);
                        FD_SET(accept_data_socket, &master_set);
                        
                        if(connect1 == -1)
                           connect1 = 0;
                        
                        printf("connect proxy:   %d\n", connect1);
                        if(cache_flag == 1 && connect1 == 1){ //如果有缓存，则模拟server直接传送文件 
                                    printf("duankailianjie\n");
                                    //断开与server的数据连接         
                                    
                                    close(connect_data_socket);
                                    FD_CLR(connect_data_socket, &master_set);
                                
                                    char send_buff[BUFFSIZE] = {0};
                               
                                    memset(send_buff, 0, sizeof(send_buff));
                                    sprintf(send_buff, "150 Opening data channel for file download from server of \"/%s.%s\"\r\n", file, type);
                                    printf("%s\n",send_buff);                           
                                    if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(150) failed.\n");
                                    }
                                
                                    memset(send_buff, 0, sizeof(send_buff));
                                    printf("connect_data_socket ok!\n");
                                    length = read (cache_fd, send_buff, 1023);  
                                    send_buff[length]  = '\0';
                                
                                    while(length > 0 && length == 1023){
                                    if(send(accept_data_socket,send_buff,1023,0) != 1023){
                                        printf("send(data)1 failed.\n");
                                    }
                                    length = read (cache_fd, send_buff, 1023);  
                                    }
                                
                                    if(send(accept_data_socket, send_buff, length, 0) != length){
                                    printf("send(data final) failed.\n");
                                    }
                                
                                    memset(send_buff, 0, sizeof(send_buff));
                                    sprintf(send_buff, "226 Successfully transferred \"/%s.%s\"\r\n", file, type);
                                    if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(226) failed.\n");
                                    }
                                    close(cache_fd);
                                
                                    close(proxy_data_socket);
                                    FD_CLR(proxy_data_socket, &master_set);
                                    close(accept_data_socket);
                                    FD_CLR(accept_data_socket, &master_set);
                                    cache_flag = -1;
                                    RETR_flag = -1; 
                                    connect1 = -1;
                                } else 
                                  connect1 = -1;
                        printf("out proxt_data_socket\n"); 
                        //在建立proxy与client的连接后 
                        
                    } else {
                        printf("Error!\n");
                        //remove all from the master_set
                        close(proxy_cmd_socket);
                        FD_CLR(proxy_cmd_socket, &master_set);
                        close(accept_cmd_socket);
                        FD_CLR(accept_cmd_socket, &master_set);
                        close(connect_cmd_socket);
                        FD_CLR(connect_cmd_socket, &master_set);
                        close(proxy_data_socket);
                        FD_CLR(proxy_data_socket, &master_set);
                        close(accept_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        close(connect_data_socket);
                        FD_CLR(connect_data_socket, &master_set);
                    }
                } 
                //主动模式：server来的数据；被动模式：client来的数据       
                if (i == accept_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    length = read(i, buff_data, BUFFSIZE);
                    printf("accept_data_socket\n");
                    //if ((length = read(i, buff_data, BUFFSIZE)) == 0) {
                    //    close(i); //如果接收不到内容,则关闭Socket
                    //    close(accept_data_socket);  
                    //    //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                    //    FD_CLR(i, &master_set);
                    //    FD_CLR(accept_data_socket, &master_set);
                    //} else 
                    if(RETR_flag == 1){ //从server下载 ，同时缓存进cache文件夹   
                        if(cache_flag == 0 && type_flag == 1){ //如果是pdf,jpg则缓存 
                            if((cache_fd = creat(cachePath, 0644)) == -1){
			                    printf("Error in opening\n");
            	      	        //close(sock);
                   			    exit(1);
                            }
                        } else {
                        }
   
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("%s\n", "send() error!");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //写入结束符，结束读写 
                        if(cache_flag == 0 && type_flag == 1)
                            write(cache_fd,'\0',1);
                        close(cache_fd);
                        
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
                        
                        RETR_flag = -1;
    
                    } else if(RETR_flag == 0){ //client上传 
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("454 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            } 
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
                        
                        RETR_flag = -1;
                    } else { //非上传下载文件则直接传输内容 
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("475 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
    
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
                    }   
                        //判断主被动和传输方式（上传、下载）决定如何传输数据
                }   
                if (i == connect_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    printf("connect_data_socket:\n");
                    length = read(i, buff_data, BUFFSIZE);
                    //if ((length = read(i, buff_data, BUFFSIZE)) == 0) {
                    //    close(i); //如果接收不到内容,则关闭Socket
                    //    close(connect_data_socket);     
                    //    //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                    //    FD_CLR(i, &master_set);
                    //    FD_CLR(connect_data_socket, &master_set);
                    //} else 
                    if(RETR_flag == 0){ //从client上传
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("506 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
                        
                        RETR_flag = -1;
                        
                    }else if(RETR_flag == 1){ //从server下载 
                        if(cache_flag == 0 && type_flag == 1){ //如果是pdf,jpg则缓存 
                            if((cache_fd = creat(cachePath, 0644)) == -1){
			                    printf("Error in opening\n");
            	      	        //close(sock);
                   			    exit(1);
                            }
                        } else {
                        }
                        
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("528 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //把文件结束符写入，结束文件读写 
                        if(cache_flag == 0 && type_flag == 1)
                            write(cache_fd,'\0',1);  
                        close(cache_fd);   
                        
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
                        
                        RETR_flag = -1;
                        
                    } else{ //非上传下载文件则直接传输内容 
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("550 send() failed.");
                                close(i); //如果发送错误,则关闭Socket
                                close(connect_data_socket);
                                //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        close(proxy_data_socket);
                        close(accept_data_socket);
                        close(connect_data_socket);
                        FD_CLR(accept_data_socket, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        FD_CLR(proxy_data_socket, &master_set);
        
                    }                     
                }   
            }
        }
    }
    return 0;
}

int get_free_port()
{
    int port = 0;
    int fd = -1;
    port = -1;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0); //电脑随机分配，则为0 
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ 
         return -1;
    }
    
    if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0){ //bind已获得系统分配的随机端口 
        close(fd);
        return -1;
    }

    int len = sizeof(sin);
    if(getsockname(fd, (struct sockaddr *)&sin, &len) != 0){ //获取socket对应的struct的信息 
        close(fd);
        return -1;
    }

    port = sin.sin_port; //获取port number 
    if(fd != -1)         //最后如果fd没有关闭的话则关闭fd 
        close(fd);
        
    return port;
}
