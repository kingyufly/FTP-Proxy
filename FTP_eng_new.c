#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#define BUFFSIZE 1023

#define ftpPort 21                  // the port number of FTP protocol, can be changed
#define proxyAddr "192.168.56.101"  // the address of ftp proxy
#define serverAddr "192.168.56.1"   // the address of the server

int get_free_port();

int main(int argc, const char *argv[])
{
    fd_set master_set, working_set;  //file descripter set to store all thye socket that to be seletced
    struct timeval timeout;          //the time struct in the seletc for timeout function
    int proxy_cmd_socket    = 0;     //proxy listen control connection
    int accept_cmd_socket   = 0;     //proxy accept control connection
    int connect_cmd_socket  = 0;     //proxy connect control connection
    int proxy_data_socket   = 0;     //proxy listen data connection
    int accept_data_socket  = 0;     //proxy accept data connection
    int connect_data_socket = 0;     //proxy connect data connection
    int selectResult = 0;     //the return value of seletc function
    int select_sd = 10;    //the maximum number of fd that the select function listen to
    
    int PORT_flag = -1;     //0 for standard mode; 1 for passive mode
    int RETR_flag = -1;     //0 for download(RETR); 1 for upload(STOR)
    int cache_flag = -1;      //0 for no such file in the cache; 1 for there is such file in the cache  
    int type_flag = -1;      //0 for not pdf,jpg; 1 for is pdf,jpg   
    int freePort = -1, freePortA = -1, freePortB = -1;
    int p_Connect = -1;
    
    unsigned short int clientPort; // to store the port number that the client send to the server when standard mode
    unsigned short int serverPort; // to store the port number that the server send to the client when passive mode
    
    FD_ZERO(&master_set);   //clear the master_set
    bzero(&timeout, sizeof(timeout)); // set the timeout struct to 0
    
    char clientAddr[15]; // store the clientIP address when at passive mode
    char cachePath[BUFFSIZE]; // the directory of the cache
    //char retrbuff[BUFFSIZE]; // store the RETR command for further operation 
    char file[BUFFSIZE]; // the filename of the download file (RETR filename.filetype) 
    char type[50]; // the type of the download file
    int i = 0;
    
    int value = 1; // the parameter that the setsockopt function use to enable the reuse of the port, 1 for Boolean true£¨enable£© 
    int length = 0; // the length of the read 

    int cache_fd;  //the fd of the cache check
    
    //proxy_cmd_socket = bindAndListenSocket();  //create proxy_cmd_socket and bind(),listen()
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

    FD_SET(proxy_cmd_socket, &master_set);  //add proxy_cmd_socket into master_set
    
    timeout.tv_sec = NULL;     //s, the timeout of the Select()
    timeout.tv_usec = NULL;    //ms
    
    while (1) {
        FD_ZERO(&working_set); //clear all socket in the working_set
        memcpy(&working_set, &master_set, sizeof(master_set)); //copy the master_set into theworking_set
        
        //select() listen all the socket, for this case, only listen the read operation (data come to the port)
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
        // selectResult > 0, start to check which socket have data comes
        for (i = 0; i < select_sd; i++) {
            //judge the changing fd is whether in the working_set
            if (FD_ISSET(i, &working_set)) {
                // establish the control connection client->proxy->server 
                if (i == proxy_cmd_socket) { 
                    //accept_cmd_socket = acceptCmdSocket();  //accept(), establish the connection between client and proxy
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
                    
                    //connect_cmd_socket = connectToServer(); 
                    //connect(), establish the connection between proxy and server     
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
                        
                    //add the new socket into the master_set
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }   
                //handle the commands from the client
                if (i == accept_cmd_socket) {
                    char buff[BUFFSIZE] = {0};
                       
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        close(i); //if not receive any data then close the socket
                        close(connect_cmd_socket);
                        //after close the socket£¬remove the socket from the master_set
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);
                    } else {
                        buff[length] = '\0';
                        
                        //PORT command, get the IP address of the client, port number, change IP port according to the proxy
                        if(strstr(buff,"PORT") != NULL){
                            PORT_flag = 1;
                            char* portchar = strtok(buff, " ");
                            char* ip1 = strtok(NULL, ",");
                            char* ip2 = strtok(NULL, ",");
                            char* ip3 = strtok(NULL, ",");
                            char* ip4 = strtok(NULL, ",");
                            char* portA = strtok(NULL, ",");
                            char* portB = strtok(NULL, "\r\n");
    
                            sprintf(clientAddr,"%s.%s.%s.%s",ip1,ip2,ip3,ip4); //get the clientIP address 
                            clientPort = atoi(portA)*256 + atoi(portB);
                     
                            freePort = get_free_port(); //get available port from the proxy for the data connection
                            freePortA = freePort/256;
                            freePortB = freePort%256;
    
                            //send the modified PORT command to the server
                            sprintf(buff, "PORT %d,%d,%d,%d,%d,%d\r\n",192,168,56,101,freePortA,freePortB);
                              
                            if(send(connect_cmd_socket, buff, sizeof(buff), 0) != sizeof(buff)){
                                printf("%s\n", "send() error!");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              
                            //create the proxy_data_soket to listen the data connection from the server (standard mode), client (passive mode)
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
                            //add proxy_data_socket into master_set
                            FD_SET(proxy_data_socket, &master_set);
                        }    
                        // check PASV command, if received PASV command from the client, then change the PORT_flag to 0 for passive mode
                        else if(strstr(buff,"PASV") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("210 send() failed.\n");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                            PORT_flag = 0;
                        }
                        //check RETR command, get the filename and the filetype, then check filetype 
                        //and the file is whether in the cache
                        else if(strstr(buff,"RETR ") != NULL){
                            char* retrChar = strtok(buff, " ");
                            char* fileName = strtok(NULL, ".");
                            char* fileType = strtok(NULL, "\r\n");
                            
                            strcpy(file, fileName);
                            strcpy(type, fileType);
                            
                            if((strstr(type,"txt") != NULL) || (strstr(type,"jpg") != NULL)|| (strstr(type,"TXT") != NULL) || (strstr(type,"JPG") != NULL))
                                type_flag = 1;
                            else 
                                type_flag = -1;
                                
                            sprintf(buff, "%s %s.%s\r\n", "RETR", file, type);
                            sprintf(cachePath, "/root/cache/%s.%s", file, type);

                            if((cache_fd = open(cachePath, O_RDONLY, 0)) == -1){  
            		            printf("no such file\n");
            		            cache_flag = 0;
            		            close(cache_fd);
            		            if(send(connect_cmd_socket, buff, length, 0) != length){
                                    printf("220 send() failed.\n");
                                    close(i);
                                    close(connect_cmd_socket);
                                    FD_CLR(i, &master_set);
                                    FD_CLR(connect_cmd_socket, &master_set);
                                }
                                RETR_flag = 1;
                                
        	                }else if(PORT_flag == 1){ //standard mode, cache
        	                // does not send the RETR command to the server, and the proxy simulate 
                            // the server to compelete the file transmission by sending 150 -> connect 
                            // to client, tranmit file -> send 226 -> close connection
                                printf("there is such file\n");
                                cache_flag = 1;   
                                /////////////////////////////////////////

                                RETR_flag = 1;
                                if ((connect_data_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
                                    printf("356 socket() failed.\n");
    	                            exit(1);
                                }
                        
                                // set the data connection port of the proxy is always be (control connection port -1)
                                // enable socket reuse the port number
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
                                
                                //send 150 before connection and file transmission
                                memset(send_buff, 0, sizeof(send_buff));
                                sprintf(send_buff, "150 Opening data channel for file download from server of \"/%s.%s\"\r\n\0", file, type);                       
                                if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(150) failed.\n");
                                }
                                
                                
                                if(connect(connect_data_socket,(struct sockaddr *)&connect_data_struct,sizeof(connect_data_struct)) == -1){
                                    printf("366 connect() failed.\n");
    	                            close(connect_data_socket);
    	                            exit(1);
                                }
                                
                                
                                //return file from the cache folder
                                memset(send_buff, 0, sizeof(send_buff));
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
                                
                                
                                //sedn 226 after finish transmission
                                memset(send_buff, 0, sizeof(send_buff));
                                sprintf(send_buff, "226 Successfully transferred \"/%s.%s\"\r\n\0", file, type);
                                if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                    printf("send(226) failed.\n");
                                }
                                
                                //close connection and remove the corresponding socket from the master_set
                                close(cache_fd);
                                close(connect_data_socket);
                                close(proxy_data_socket);
                                FD_CLR(proxy_data_socket, &master_set);
                                
                                cache_flag = -1;
                                break;
                            } else if (PORT_flag == 0){
                                RETR_flag = 1;
                                cache_flag = 1;
     
                                if(cache_flag == 1 && p_Connect == -1) //the first time that passive mode data connection before proxy_data_socket
                                    p_Connect = 1;
                                if(cache_flag == 1 && p_Connect == 0){
                                    close(connect_data_socket);
                                    FD_CLR(connect_data_socket, &master_set);
                                
                                    char send_buff[BUFFSIZE] = {0};
                               
                                    memset(send_buff, 0, sizeof(send_buff));
                                    sprintf(send_buff, "150 Opening data channel for file download from server of \"/%s.%s\"\r\n", file, type);                        
                                    if(send(accept_cmd_socket, send_buff, strlen(send_buff), 0) != strlen(send_buff)){
                                        printf("send(150) failed.\n");
                                    }
                                
                                    memset(send_buff, 0, sizeof(send_buff));
                                    length = read (cache_fd, send_buff, 1023);  
                                    send_buff[length]  = '\0';
                                
                                    while(length > 0 && length == 1023){
                                    if(send(accept_data_socket,send_buff,1023,0) != 1023){
                                        printf("send(data)0 failed.\n");
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
                                    p_Connect = -1;
                                }
                            }        
                        }
                        //check STOR, and change the RETR_flag to 0 for upload
                        else if(strstr(buff,"STOR") != NULL){
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("231 send() failed.\n");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                              RETR_flag = 0;
                        }
                                
                        //except RETR PORT PASV STOR, all the data are delivered directly
                        else{
                            if(send(connect_cmd_socket, buff, length, 0) != length){
                                printf("245 send() failed.\n");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }
                    }   
                }  
                if (i == connect_cmd_socket) {
                    // handle the command that the server send back to the client
                    char buff[BUFFSIZE] = {0};
                    
                    if ((length = read(i, buff, BUFFSIZE)) == 0) {
                        printf("close socket!\n");
                        close(i);
                        close(accept_cmd_socket);     
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket, &master_set);
                    } else {
                        buff[length] = '\0';
                        // PASV reply, get the server's IP address and port number
                        // change to local random port number and proxy IP address
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
                              
                            // send the modified command to the client 
                            sprintf(buff, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",192,168,56,101,freePortA,freePortB);
    
                            //listen for data connection
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
                            FD_SET(proxy_data_socket, &master_set);//add proxy_data_socket into master_set
                            
                            //listen before sedn, in case of not receiveing the connection
                            if(send(accept_cmd_socket, buff, strlen(buff), 0) != strlen(buff)){
                                printf("314 send() failed.");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }
                        }
                        //other situation just deliver the data directly
                        else{
                            if(send(accept_cmd_socket, buff, length, 0) != length){
                                printf("324 send() failed.");
                                close(i);
                                close(connect_cmd_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_cmd_socket, &master_set);
                            }  
                        }    
                    }
                }
                // when the proxy_data_socket is selected it means that whether 
                // server or client have start the data connection to the proxy
                if (i == proxy_data_socket) {
                    // standard mode, accept the connection from the server ad connect to the 
                    // client according to the PORT command' IP and port number
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
                        
                        //add the new socket into master_set
                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                    } else if(PORT_flag == 0){
                        // passive mode, accept the connection from the client, and 
                        // then connect to the server according to the 227 PASV reply
                        // if the file has been cached, then disconnect the data connection 
                        // and simulate the server to send the file back to client
                        // 150 -> send -> 226 -> close connection 
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
                        
                        if(p_Connect == -1)
                           p_Connect = 0;
                        
                        //after establishing the connection with the server
                        if(cache_flag == 1 && p_Connect == 1){
                            //if the file has been cached, then disconnect the connection_data_socket
                                    
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
                            p_Connect = -1;
                        } else 
                            p_Connect = -1;
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
                if (i == accept_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    length = read(i, buff_data, BUFFSIZE);

                    if(RETR_flag == 1){ //download from the server while cache the file
                        if(cache_flag == 0 && type_flag == 1){ //cache the file if type is jpg, pdf
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
                                close(connect_data_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //write the EOF to edn the file write
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
    
                    } else if(RETR_flag == 0){ //upload from the client, standard mode
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("454 send() failed.");
                                close(i); 
                                close(connect_data_socket);
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
                    } else { 
                        // if not file operation, just transfer the data directly
                        do{
                            buff_data[length] = '\0';
                            if(send(connect_data_socket, buff_data, length, 0) != length){
                                printf("475 send() failed.");
                                close(i); 
                                close(connect_data_socket);
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
                if (i == connect_data_socket) {
                    char buff_data[BUFFSIZE] = {0};
                    length = read(i, buff_data, BUFFSIZE);
                    if(RETR_flag == 0){ //upload from the client, standard mode
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("506 send() failed.");
                                close(i);
                                close(connect_data_socket);
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
                        
                    }else if(RETR_flag == 1){ //downloading from the server, passive mode
                        if(cache_flag == 0 && type_flag == 1){ 
                        //if the file is pdf,jpg format, the file is to be cached
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
                                close(i); 
                                close(connect_data_socket);
                                FD_CLR(i, &master_set);
                                FD_CLR(connect_data_socket, &master_set);
                            }
                            if(cache_flag == 0 && type_flag == 1)
                                write(cache_fd,buff_data,length);
                        }while((length = read(i, buff_data, BUFFSIZE)) != 0);
                        
                        //write the EOF, to end the file
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
                        
                    } else{ 
                    // if not file operation, just transfer the data directly
                        do{
                            buff_data[length] = '\0';
                            if(send(accept_data_socket, buff_data, length, 0) != length){
                                printf("550 send() failed.");
                                close(i);
                                close(connect_data_socket);
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
    sin.sin_port = htons(0); //set 0 for random allocation
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ 
         return -1;
    }
    
    if(bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0){ //bind() to get the random port number that the system allocated
        close(fd);
        return -1;
    }

    int len = sizeof(sin);
    if(getsockname(fd, (struct sockaddr *)&sin, &len) != 0){ //get the correcponding struct information including the port number
        close(fd);
        return -1;
    }

    port = sin.sin_port; //get the port number 
    if(fd != -1)         //close the fd if not closed before
        close(fd);
        
    return port;
}
