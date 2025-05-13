#include<stdio.h>
#include<string.h>
#include<stdlib.h>	
#include<sys/socket.h>
#include <netdb.h>
#include<arpa/inet.h>	
#include<unistd.h>
#include<signal.h> 
#include<time.h>
#include<sys/time.h>
#include<openssl/md5.h>
#include<stdbool.h>
#include<sys/file.h>
#include<regex.h>
#include <sys/wait.h>


void handle_errors(int res, char *loc){
    if(res < 0) printf("error with %s\n", loc);
}

//handle recv errors
void handle_recv(int read_size){
    if(read_size == 0){
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1){
		perror("recv failed");
	}
}

void sig_int_handler(int sig_num){
    printf("Control-C is recieved.\nNo new connections will be accepted\n");
    while (wait(NULL) > 0);
    exit(0);
}

//returns 1 if file exist + is timed in and 0 if it doesn't exist or is timed out
int search_dir(char file_path[], int timer, int lock){
    char line[200];
    FILE *file_ptr = fdopen(lock, "r");
    if(!file_ptr){
        return 0;
    }
    fgets(line, sizeof(line), file_ptr);
    fclose(file_ptr);
    time_t file_time = atol(line);   
    time_t now = time(NULL);               
    double age = difftime(now, file_time);
    if(age < timer){
        return 1;
    }
    return 0;
}

//create a hash based on file name 
void make_hash(char file_name[], char hash[]){
    unsigned char digest[MD5_DIGEST_LENGTH];
    unsigned char md5_string[2 * MD5_DIGEST_LENGTH + 1]; 
    MD5((unsigned char *)file_name, strlen(file_name), digest);
    // Convert to hex string
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        sprintf(&md5_string[i * 2], "%02x", digest[i]);
    }
    md5_string[32] = '\0';
    printf("MD5 of '%s' = %s\n", file_name, md5_string);
    strcpy(hash, md5_string);
}

//send the given file
void send_file(char filepath[], int client_sock, int lock){
    lock = open(filepath, O_CREAT | O_RDWR, 0666);
    FILE *file_ptr = fdopen(lock, "r");
    char read_buffer[2000], time[2000];
    size_t read_bytes; 
    int count = 1;
    //extract the time from the file
    fgets(time, sizeof(time), file_ptr);
    while((read_bytes =  fread(read_buffer, 1, 2000, file_ptr)) > 0){
        int total_sent = 0;
        while(total_sent < read_bytes){
            int sent = send(client_sock, read_buffer + total_sent, read_bytes - total_sent, 0);
            if(sent < 0) exit(1);
            total_sent += sent;
        } 
    }
    fclose(file_ptr);
}

//parse the incoming request from the client to the proxy
void request_parser(char request_host[], char request_portno[], char request_path[], char request_body[], char full_path[],char client_message[], int *connection){
    int count = 0, host_flag = 0;
    char mess_cpy[2000], mess_cpy2[2000], mess_cpy3[2000]; 
    strcpy(mess_cpy, client_message);
    strcpy(mess_cpy2, client_message);
    memset(request_host, 0, 256);
    memset(request_portno, 0, 5);
    memset(request_path, 0, 2000);
    memset(request_body, 0, 2000);
    char *path = strtok(mess_cpy, " \r\n");
    //set the portnumber and request path
    if(!strcmp(path, "GET")){
        while(path != NULL){
            count++;
            if(count == 2){
                strcpy(full_path, path);
                if(strstr(path, "://")){
                    char *temp = strstr(path, "://"); 
                    temp += 3;
                    char *portno = strstr(temp, ":");
                    if(portno){
                        portno++;
                        strcpy(request_portno, portno);
                        request_portno[strlen(request_portno) - strlen(strstr(portno, "/"))] = '\0';
                        printf("PORTNO: %s\n", request_portno);
                    }else{
                        strcpy(request_portno, "80");
                    } 
                    temp = strstr(temp, "/");
                    strcpy(request_path, temp); 
                }
            }
            path = strtok(NULL, " \r\n");
        }
        //set the host
        char *word = strtok(client_message, " \r\n");
        while(word != NULL){
            //if a host exists
            if(!strncasecmp(word, "HOST:", 5)) {    
                word += strlen("HOST:");
                while(*word == ' '){word++;}
                strcpy(request_host, word);
                host_flag = 1;
            }
            word = strtok(NULL, "\r\n");
        }
        if(!host_flag){
            char *temp_host = strstr(mess_cpy3, "http://");
            if(temp_host){
                temp_host += 7;
                char host_buf[2000];
                strcpy(host_buf, temp_host);
                char *host = strtok(host_buf, "/ ");
                if(host){
                    printf("temphost: %s\n", host);
                    strcpy(request_host, host);
                }
            }
        }
        //testing for complete request with body
        if(strstr(mess_cpy2, "\r\n\r\n")){
            char *body = strstr(mess_cpy2, "\r\n\r\n");
            body += 4;
            strcpy(request_body, body);
        }else{
            strcpy(request_host, "400 Bad Request");
        }
        strtok(mess_cpy2, "\r\n");
        if(!strstr(mess_cpy2, "HTTP/1.1") && !strstr(mess_cpy2, "HTTP/1.0")){
            strcpy(request_host, "400 Bad Request");
        }
        if(strstr(mess_cpy2, "Keep-Alive")){
           *connection = 1;
        }else{
            *connection = 0;
        }
    }else{
        printf("Bad request!\n");
        strcpy(request_host, "400 Bad Request");
    }
}

//create the header to serve to client from proxy
void header_parser(char header[], char content_length[]){
    char headercpy[9000];
    char result[2000] = {0};
    char *line;
    int found_length = 0, found_type = 0;
    strcpy(headercpy, header);
    line = strtok(headercpy, "\r\n");
    while(line){
        if(result[0] == '\0'){
            strcat(result, line);
            strcat(result, "\r\n");
        }else if(!found_length && strncasecmp(line, "Content-Length:", 15) == 0) {
            strcat(result, line);
            strcat(result, "\r\n");
            strcpy(content_length, line + 15);
            found_length = 1;
        }else if(!found_type && strncasecmp(line, "Content-Type:", 13) == 0) {
            strcat(result, line);
            strcat(result, "\r\n");
            found_type = 1;
        }
        line = strtok(NULL, "\r\n");
    }
    strcat(result, "\r\n");
    strcpy(header, result);
}

//caches file into hash table
void cache_file(int socket_desc, char request_path[], int timer, int lock){
    char client_message[2000], file_path[2000], md5_string[2000], curr_time[9], header[9000];
    char *header_end;
    int n = 0, bytes_rec = 0 , header_flag = 1, breaker = 0;
    time_t now = time(NULL);
    sprintf(curr_time, "%ld\n", now);
    make_hash(request_path, md5_string);
    sprintf(file_path, "./cache/%s", md5_string);
    FILE *new_file = fopen(file_path, "w");
    char length[20];
    fwrite(curr_time, 1, strlen(curr_time), new_file);
    fflush(new_file);
    while((n = recv(socket_desc , client_message , 2000 , 0)) > 0){
        breaker += n;
        printf("bytes: %d\n", n);
        if(header_flag){
            memcpy(header + bytes_rec, client_message, n);
            bytes_rec += n;
            header[bytes_rec] = '\0';
            //write the required headers into file and any body information
            if((header_end = strstr(header, "\r\n\r\n"))){
                size_t header_size = header_end - header + 4;
                size_t leftover = bytes_rec - header_size;
                header_parser(header, length);
                fwrite(header, 1, strlen(header), new_file);
                fwrite(header_end + 4, 1, leftover, new_file);
                fflush(new_file);
                header_flag = 0;
            }
        }else{
            //write the rest of the body into file
            fwrite(client_message, 1, n, new_file);
            fflush(new_file);
            if(breaker >= (atoi(length) + strlen(header))) break;
        }
    }
    fclose(new_file);
    printf("done caching\n");
}

int make_socket(int request_portno, struct hostent *request_host, char forwarding_request[], char dynamic_check[],char request_path[], int timer, int lock){
    printf("MAKING SOCKET\n");
    int socket_desc , client_sock , c , read_size, error, portno;
	struct sockaddr_in server , client;
    struct timeval timeout = {5, 0};
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	handle_errors(socket_desc, "socket");
    int optval = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(socket_desc);
        exit(EXIT_FAILURE);
    }
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_port = htons(request_portno);
    memcpy(&server.sin_addr.s_addr, request_host->h_addr_list[0], request_host->h_length);
	//Connect
	handle_errors(connect(socket_desc,(struct sockaddr *)&server , sizeof(server)), "bind");
    int bytes_sent = send(socket_desc , forwarding_request , strlen(forwarding_request), 0);
    handle_errors(bytes_sent, "send");
    //check if page is dynamic and cache the file if necessary
    if(strchr(dynamic_check, '?') == NULL){
        cache_file(socket_desc, request_path, timer, lock);
    }
}

//create the request to send to server
void create_request(char forwarding_req[], char request_path[], char request_host[]){
    char *method = strtok(forwarding_req, " ");
    strtok(NULL, " ");
    char *http = strtok(NULL, "\r\n");
    sprintf(forwarding_req, "%s %s %s\r\nHost: %s\r\n\r\n", method, request_path, http, request_host);
}


int main(int argc , char *argv[]){
	int socket_desc , client_sock , c , read_size, error, portno, timer;
    struct hostent *host_ip;
	struct sockaddr_in server , client;
	char client_message[2000], client_message_copy[2000], request_host[256], request_portno[5], request_path[2000], request_body[2000], full_path[2000];
    pid_t childpid;
    portno = atoi(argv[1]);
    timer = atoi(argv[2]);
    struct timeval timeout = {5, 0};
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	handle_errors(socket_desc, "socket");
    int optval = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(socket_desc);
        exit(EXIT_FAILURE);
    }
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( portno );
	//Bind
	handle_errors( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)), "bind");
	//Listen
	listen(socket_desc , 10);
	//Accept and incoming connection
	printf("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	signal(SIGINT, sig_int_handler);
    while(1){
        pid_t sid = setsid();
        int connection = 0;
        //accept connection from an incoming client
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);

        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        handle_errors(client_sock, "client accept");
        puts("Connection accepted");
        if((childpid = fork()) == 0){
            printf("Process: %d created\n", getpid());
            //Receive a message from client
            while(1){
                read_size = recv(client_sock , client_message , 2000 , 0);
                handle_recv(read_size);
                strcpy(client_message_copy, client_message);
                printf("Recieved Header: %s\n", client_message);
                //parse recieved string
                request_parser(request_host, request_portno, request_path, request_body, full_path, client_message, &connection);
                if(!strcmp(request_host, "400 Bad Request")){
                    printf("bad request");
                    char *err = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\nContent-Type: text/plain\r\n\r\n400 Bad Request";
                    write(client_sock, err, strlen(err));
                    
                    write(client_sock , err , strlen(err));
                    close(client_sock);
                    _exit(0); 
                }else if((host_ip = gethostbyname(request_host)) != NULL){
                    printf("Host was found\n");
                    char hash[2000], hashed_path[2000] = "./cache/";
                    make_hash(full_path, hash);
                    strcat(hashed_path, hash);
                    int lock = open(hashed_path, O_CREAT | O_RDWR, 0666);
                    //lock the newly created file so no other processes with same lookup cant run
                    flock(lock, LOCK_EX);
                    if(!search_dir(hashed_path, timer, lock)){
                        printf("REACHING OUT TO SERVER FOR FILE\n");
                        create_request(client_message_copy, request_path, request_host);
                        make_socket(atoi(request_portno), host_ip, client_message_copy, request_path, full_path, timer, lock);
                        send_file(hashed_path, client_sock,lock);
                        printf("file sent\n");
                    }else{
                        //cached file can be sent 
                        printf("SENDING CACHED FILE\n");
                        send_file(hashed_path, client_sock, lock);
                    }
                    flock(lock, LOCK_UN);
                    close(lock);
                }else{
                    printf("host not found\n");
                    char *err = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
                    write(client_sock , err , strlen(err));
                    printf("bad request");
                    close(client_sock);
                    _exit(0);
                }
                if(!connection){
                    break;
                }
            }
            close(client_sock);
            _exit(0);
        }
        printf("XITED FORK\n");
        close(client_sock);
    }
}