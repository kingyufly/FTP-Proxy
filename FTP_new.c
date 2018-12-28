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
    fd_set master_set, working_set;  //�ļ�����������
    struct timeval timeout;          //select �����еĳ�ʱ�ṹ��
    int proxy_cmd_socket    = 0;     //proxy listen��������
    int accept_cmd_socket   = 0;     //proxy accept�ͻ�������Ŀ�������
    int connect_cmd_socket  = 0;     //proxy connect������������������
    int proxy_data_socket   = 0;     //proxy listen��������
    int accept_data_socket  = 0;     //proxy accept�õ�������������ӣ�����ģʽʱaccept�õ��������������ӵ����󣬱���ģʽʱaccept�õ��ͻ����������ӵ�����
    int connect_data_socket = 0;     //proxy connect������������ ������ģʽʱconnect�ͻ��˽����������ӣ�����ģʽʱconnect�������˽����������ӣ�
    int selectResult = 0;     //select��������ֵ
    int select_sd = 10;    //select ��������������ļ�������
    
    int PORT_flag = -1;     //0 for standard mode; 1 for passive mode
    int RETR_flag = -1;     //0 for download(RETR); 1 for upload(STOR)
    int cache_flag = -1;      //0 for no such file in the cache; 1 for there is such file in the cache  
    int type_flag = -1;      //0 for not pdf,jpg; 1 for is pdf,jpg   
    int freePort = -1, freePortA = -1, freePortB = -1;
    int connect1 = -1;

    unsigned short int clientPort; // ������������ģʽʱ��port�� 
    unsigned short int serverPort; // �������汻��ģʽʱ��pasv�������صĶ˿ںţ� 
    
    FD_ZERO(&master_set);   //���master_set����
    bzero(&timeout, sizeof(timeout));
    
    char clientAddr[15]; // portģʽʱ������clientIP address  
    char cachePath[BUFFSIZE]; // cache ·�� 
    char retrbuff[BUFFSIZE]; // passive ģʽʱRETR����Ĵ洢�����ڴ��� 
    char file[BUFFSIZE]; // RETR ���ļ��� 
    char type[50]; // RETR ���ļ����� 
    int i = 0;
    
    int value = 1; //setsockoptʱʹ��ԭΪBooleanֵtrue��enable�� 
    int length = 0; // buffer �����ĳ��� 

    int cache_fd;  //�鿴cache���ļ���ʶ�� 
    
    //proxy_cmd_socket = bindAndListenSocket();  //����proxy_cmd_socket��bind������listen����
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

    FD_SET(proxy_cmd_socket, &master_set);  //��proxy_cmd_socket����master_set����
    
    timeout.tv_sec = NULL;    //Select�ĳ�ʱ����ʱ��
    timeout.tv_usec = NULL;    //ms
    
    while (1) {
        FD_ZERO(&working_set); //���working_set�ļ�����������
        memcpy(&working_set, &master_set, sizeof(master_set)); //��master_set����copy��working_set����
        
        //selectѭ������ ����ֻ�Զ������ı仯���м�����working_setΪ���Ӷ������������������ļ��ϣ�,�����͵��ĸ�������NULL������д����������������м���
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
        // selectResult > 0 ʱ ����ѭ���ж��б仯���ļ�������Ϊ�ĸ�socket
        for (i = 0; i < select_sd; i++) {
            //�жϱ仯���ļ��������Ƿ������working_set����
            if (FD_ISSET(i, &working_set)) {
                // ������������ client->proxy->server 
                if (i == proxy_cmd_socket) { 
                    //accept_cmd_socket = acceptCmdSocket();  //ִ��accept����,����proxy�Ϳͻ���֮��Ŀ�������
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
                    
                    //connect_cmd_socket = connectToServer(); //ִ��connect����,����proxy�ͷ�������֮��Ŀ�������       
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
                        
                    //���µõ���socket���뵽master_set�����
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }   
                //client����command�Ĵ���    
                if (i == accept_cmd_socket) {
                    printf("accept_cmd_socket\n");
                    char buff[BUFFSIZE] = {0};
                       
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        close(i); //������ղ�������,��ر�Socket
                        close(connect_cmd_socket);
                        //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);
                    } else {
                        printf("accept_cmd_socket: %s\n",buff);
                        buff[length] = '\0';
                        //������յ�����,������ݽ��б�Ҫ�Ĵ���֮���͸��������ˣ�д��connect_cmd_socket��
                        
                        //port ��� IP address ��ȡ�� port��ȡ 
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
    
                            sprintf(clientAddr,"%s.%s.%s.%s",ip1,ip2,ip3,ip4); //���clientIP address 
                            clientPort = atoi(portA)*256 + atoi(portB);
                     
                            freePort = get_free_port(); //ѡȡ���ö˿� 
                            freePortA = freePort/256;
                            freePortB = freePort%256;
    
                            //��Ϊclient��proxyΪͬһIP������ֱ����clientIP��Ӧ�ø�Ϊproxy�Ĺ̶�IP 
                            sprintf(buff, "PORT %d,%d,%d,%d,%d,%d\r\n",192,168,56,101,freePortA,freePortB);
                              
                            if(send(connect_cmd_socket, buff, sizeof(buff), 0) != sizeof(buff)){
                                printf("%s\n", "send() error!");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              
                            //�����˿�
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
                            FD_SET(proxy_data_socket, &master_set);//��proxy_data_socket����master_set����
                            printf("proxy_data_socket ok\n");
                        }    
                        // pasv������� 
                        else if(strstr(buff,"PASV") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("210 send() failed.\n");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                            PORT_flag = 0;
                        }
                        //RETR��������� ��ȡ�ļ������������ж��Ƿ��л��棬��������\����ģʽ����ִ�� 
                        else if(strstr(buff,"RETR ") != NULL){//��access����
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
                                    close(i); //������ʹ���,��ر�Socket
                                    close(connect_cmd_socket);
                                    //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                    FD_CLR(i, &master_set);
                                    FD_CLR(connect_cmd_socket, &master_set);
                                }
                                RETR_flag = 1;
                                
        	                }else if(PORT_flag == 1){ //portģʽ������RETR���ģ��server��client�������ݴ��� 

                                printf("there is such file\n");
                                cache_flag = 1;   
                                /////////////////////////////////////////

                                RETR_flag = 1;
                                if ((connect_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                                    printf("356 socket() failed.\n");
    	                            exit(1);
                                }
                        
                                //����socketѡ�ʹ�ö˿�close���ܹ��ٴ�bindͬһ�˿ڣ���
                                //������ģʽʱ��proxyʼ���ԣ�21-1���˿�����client 
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
                                
                                //�����ļ�ǰ����150 
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
                                
                                
                                //��cache�з����ļ�
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
                                
                                
                                //�����ļ�����226ȷ�� 
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
                                if(cache_flag == 1 && connect1 == 0){ //����л��棬��ģ��serverֱ�Ӵ����ļ� 
                                    printf("duankailianjie\n");
                                    //�Ͽ���server����������         
                                    
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
                        //�ϴ��ļ�ʱ���ж�ֱ��ת�� 
                        else if(strstr(buff,"STOR") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("231 send() failed.\n");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              RETR_flag = 0;
                        }
                                
                        //д��proxy��server������cmd����,����PORT֮�⣬ֱ��ת��buff����
                        //write(connect_cmd_socket), buff, strlen(buff));
                        else{
                            printf("esle: %s\n",buff);
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("245 send() failed.\n");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }
                    }   
                }  
                if (i == connect_cmd_socket) { //server��������� 
                    
                    char buff[BUFFSIZE] = {0};
                    
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        printf("close socket!\n");
                        close(i); //������ղ�������,��ر�Socket
                        close(accept_cmd_socket);     
                        //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket, &master_set);
                    } else {
                        buff[length] = '\0';
                        printf("connect_cmd_socket:%s\n",buff);  
                        //����������˷���proxy��reply��д��accept_cmd_socket
                        //passive mode ʱ����227��� ��ȡport���ȼ�������ı�ip��port������client 
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
                              
                            //��Ϊclient��proxyΪͬһIP������ֱ����clientIP��Ӧ�ø�Ϊproxy�Ĺ̶�IP 
                            sprintf(buff, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",192,168,56,101,freePortA,freePortB);
    
                            //�����˿�
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
                            FD_SET(proxy_data_socket, &master_set);//��proxy_data_socket����master_set����
                            
                            //�Ƚ��м����ٷ���pasv�����Է�connection refused 
                            if(send(accept_cmd_socket, buff, strlen(buff), 0) != strlen(buff)){
                                printf("314 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                            printf("passive send ok!\n");
                        } else if(strstr(buff,"226 ABOR command successful") != NULL){
                            printf("jiedaole 226"); 
                        } 
                        //����ֱ��ת�� 
                        else{
                            if(send(accept_cmd_socket, buff, length, 0) != length){
                                printf("324 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_cmd_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }    
                    }
                }
                //�����˿ڣ�����ģʽ����20�˿ڣ�����ģʽ���������˿� 
                if (i == proxy_data_socket) {
                    printf("proxy_data_socket\n");
                    //proxy accept�õ�������������ӣ�����ģʽʱaccept�õ��������������ӵ����󣬱���ģʽʱaccept�õ��ͻ����������ӵ�����
                    //proxy connect������������ ������ģʽʱconnect�ͻ��˽����������ӣ�����ģʽʱconnect�������˽����������ӣ�
                    //����ģʽ���������ӣ���������server 
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
                        
                        //���µõ���socket���뵽master_set�����
                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                        //����data����(accept_data_socket��connect_data_socket)
                    } else if(PORT_flag == 0){ //����ģʽ���������ӣ����������cache������server�������ٶϿ��� 
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
                        if(cache_flag == 1 && connect1 == 1){ //����л��棬��ģ��serverֱ�Ӵ����ļ� 
                                    printf("duankailianjie\n");
                                    //�Ͽ���server����������         
                                    
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
                        //�ڽ���proxy��client�����Ӻ� 
                        
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
                //����ģʽ��server�������ݣ�����ģʽ��client��������       
                if (i == accept_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    length = read(i, buff_data, BUFFSIZE);
                    printf("accept_data_socket\n");
                    //if ((length = read(i, buff_data, BUFFSIZE)) == 0) {
                    //    close(i); //������ղ�������,��ر�Socket
                    //    close(accept_data_socket);  
                    //    //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                    //    FD_CLR(i, &master_set);
                    //    FD_CLR(accept_data_socket, &master_set);
                    //} else 
                    if(RETR_flag == 1){ //��server���� ��ͬʱ�����cache�ļ���   
                        if(cache_flag == 0 && type_flag == 1){ //�����pdf,jpg�򻺴� 
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
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //д���������������д 
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
    
                    } else if(RETR_flag == 0){ //client�ϴ� 
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("454 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
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
                    } else { //���ϴ������ļ���ֱ�Ӵ������� 
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("475 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
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
                        //�ж��������ʹ��䷽ʽ���ϴ������أ�������δ�������
                }   
                if (i == connect_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    printf("connect_data_socket:\n");
                    length = read(i, buff_data, BUFFSIZE);
                    //if ((length = read(i, buff_data, BUFFSIZE)) == 0) {
                    //    close(i); //������ղ�������,��ر�Socket
                    //    close(connect_data_socket);     
                    //    //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                    //    FD_CLR(i, &master_set);
                    //    FD_CLR(connect_data_socket, &master_set);
                    //} else 
                    if(RETR_flag == 0){ //��client�ϴ�
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("506 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
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
                        
                    }else if(RETR_flag == 1){ //��server���� 
                        if(cache_flag == 0 && type_flag == 1){ //�����pdf,jpg�򻺴� 
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
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //���ļ�������д�룬�����ļ���д 
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
                        
                    } else{ //���ϴ������ļ���ֱ�Ӵ������� 
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("550 send() failed.");
                                close(i); //������ʹ���,��ر�Socket
                                close(connect_data_socket);
                                //socket�رպ�ʹ��FD_CLR���رյ�socket��master_set��������ȥ,ʹ��select�������ټ����رյ�socket
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
    sin.sin_port = htons(0); //����������䣬��Ϊ0 
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ 
         return -1;
    }
    
    if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0){ //bind�ѻ��ϵͳ���������˿� 
        close(fd);
        return -1;
    }

    int len = sizeof(sin);
    if(getsockname(fd, (struct sockaddr *)&sin, &len) != 0){ //��ȡsocket��Ӧ��struct����Ϣ 
        close(fd);
        return -1;
    }

    port = sin.sin_port; //��ȡport number 
    if(fd != -1)         //������fdû�йرյĻ���ر�fd 
        close(fd);
        
    return port;
}
