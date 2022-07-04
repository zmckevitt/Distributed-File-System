/* 
 * df_server.c 
 * Zack McKevitt
 * CSCI 5273
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> 


#define MAXLINE  8192         /* max text line length */
#define MAXBUF   8192         /* max I/O buffer size */
#define LISTENQ  1024         /* second argument to listen() */

#define CONF_F   "dfs.conf"

int open_listenfd(int port);
void handle(int connfd);
void *thread(void *vargp);

char DIRNAME[50] = {0};

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc < 3) {
	fprintf(stderr, "usage: %s <directory> <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[2]);
    strcpy(DIRNAME, argv[1]);

    // If directory doesnt already exist, create it
    struct stat st;
    char *s = DIRNAME+1; //necessary to bypass leading '/' (e.g. /DFS1)
    if (stat(s, &st) == -1) {
        mkdir(s, 0700);
    }

    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
}

// thread routine
void * thread(void * vargp) 
{  
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    handle(connfd);
    close(connfd);
    return NULL;
}

// parse request, store data into filename, username, and password
// return pointer to the start of the content
char* parse_request(char* request, char* command, char* subdir, char* filename, char* username, char* password) {

    char buf[MAXLINE];
    char* s ;
    memcpy(buf, request, MAXLINE);

    // get filename from request
    s = strtok(buf+4, " ");
    strcpy(filename, s);
    s = strtok(NULL, "\r");

    // add piece number to end of filename
    strcat(filename, ".");
    strcat(filename, s);

    // add leading .  
    char temp[256];
    strcpy(temp, filename);
    sprintf(filename, ".%s", temp);


    if(strncmp(buf, "MKDIR", 5) == 0) {
        s = strtok(buf+6, "\r");
        strcpy(filename, s);   
    }

    if((s=strstr(request, "Subfolder: ")) != NULL) {
        s+=11;
        strcpy(temp, s);
        s = strtok(temp, "\r");
        strcpy(subdir, s);
    }

    if((s=strstr(request, "Username: ")) != NULL) {
        s+=10;
        strcpy(temp, s);
        s = strtok(temp, "\r");
        strcpy(username, s);
    }
    if((s=strstr(request, "Password: ")) != NULL) {
        s+=10;
        strcpy(temp, s);
        s = strtok(temp, "\r");
        strcpy(password, s);
    }
    if((s=strstr(request, "\r\n\r\n")) != NULL) {
        return s+=4;
    }
    else {
        return NULL;
    }

}

// determine if provided username/password match conf file
int validate_credentials(char* username, char* password) {
    
    FILE* fp = fopen(CONF_F, "r");
    char buf[MAXLINE];
    char *s;

    while(fgets(buf, MAXLINE, fp)) {
        
        // username present in file
        if(strncmp(username, buf, strlen(username)) == 0) {
            s = buf+strlen(username)+1;
            s = strtok(s, "\n");
            if(strcmp(password, s) == 0) {
                return 1;
            }
            else {
                return 0;
            }
        }
    }
    return 0;
}


void handle(int connfd) {
    char buf[MAXLINE]; 
    char bufcpy[MAXLINE];
    char filename[128];
    char tmp[256];
    char username[128];
    char password[128];
    char command[128];
    char subdir[128];
    char path[600];
    char* s;
    int valid, n;
    char user_directory[256];

    n = read(connfd, buf, MAXLINE);
    printf("Request from client:\n");
    

    // parse user request and tokenize
    s = parse_request(buf, command, subdir, filename, username, password);

    if(strncmp(buf, "list", 4) == 0) {

        printf("LIST\r\nSubdir: %s\r\nUsername: %s\r\nPassword: %s\n\n", subdir, username, password);

        // Wrong login info
        if(!validate_credentials(username, password)) {
            char msg[] = "Invalid credentials.";
            printf("%s\n", msg);
            write(connfd, msg, strlen(msg));
            return;
        }

        if(strncmp(subdir, "NONE", 4) != 0)
            sprintf(path, ".%s/%s/%s", DIRNAME, username, subdir);
        else
            sprintf(path, ".%s/%s/", DIRNAME, username);

        DIR *d;
        struct dirent *dir;
        d = opendir(path);

        char delim_str[257]; //strlen dir->d_name + 1
        char tmppath[1024];
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                
                // ensures no directories (skips ./ and ../)
                if(dir->d_type == DT_REG) {
                    if(dir->d_type == DT_DIR)
                        continue;
                    sprintf(delim_str, "%s ", dir->d_name);
                    write(connfd, delim_str, strlen(delim_str));
                    recv(connfd,buf,MAXLINE,0);
                }
            }
            closedir(d);
        }

    }

    if(strncmp(buf, "get", 3) == 0) {

        printf("GET %s\r\nSubdir: %s\r\nUsername: %s\r\nPassword: %s\n\n", filename, subdir, username, password);

        // Wrong login info
        if(!validate_credentials(username, password)) {
            char msg[] = "Invalid credentials.";
            printf("%s\n", msg);
            write(connfd, msg, strlen(msg));
            return;
        }

        // Subdirectory specified
        if(strncmp(subdir, "NONE", 4) != 0) {
            sprintf(path, ".%s/%s/%s", DIRNAME, username, subdir);
            path[strlen(path)-1] = '\0';

            struct stat st;
            if (stat(path, &st) == -1) {
                char msg[] = "Subfolder does not exist.";
                write(connfd, msg, strlen(msg));
                return;
            }
            sprintf(path, ".%s/%s/%s%s", DIRNAME, username, subdir, filename);
        }
        else
            sprintf(path, ".%s/%s/%s", DIRNAME, username, filename);

        FILE* fp = fopen(path, "rb");

        if(fp == NULL) {
            char msg[] = "FILE NOT FOUND";
            write(connfd, msg, strlen(msg));
            return;
        }

        while(1) {
            n = fread(buf, 1, MAXLINE, fp);
            write(connfd, buf, n);

            if(n != MAXLINE)
                break;
        }

        fclose(fp);
    }

    if(strncmp(buf, "put", 3) == 0) {

        printf("PUT %s\r\nSubdir: %s\r\nUsername: %s\r\nPassword: %s\n\n", filename, subdir, username, password);

        // Wrong login info
        if(!validate_credentials(username, password)) {
            char msg[] = "Invalid credentials.";
            printf("%s\n", msg);
            write(connfd, msg, strlen(msg));
            return;
        }


        char *dir = DIRNAME + 1; //point to dirname, but exclude leading /
        sprintf(user_directory, "%s/%s", dir, username);
        struct stat st;

        // check if user directory already exists and creates it if not
        if (stat(user_directory, &st) == -1) {
            mkdir(user_directory, 0700);
        }

        // subdirectory specified
        if(strncmp(subdir, "NONE", 4) != 0) {
            sprintf(path, "./%s/%s", user_directory, subdir);
            path[strlen(path)-1] = '\0';
            struct stat st;
            if (stat(path, &st) == -1) {
                char msg[] = "Subfolder does not exist.";
                write(connfd, msg, strlen(msg));
                return;
            }
            sprintf(path, "./%s/%s%s", user_directory, subdir, filename);
        }
        else
            sprintf(path, "./%s/%s", user_directory, filename);

        // write ACK to imply no invalid credentials
        write(connfd, "ACK", 4);

        FILE *fp = fopen(path, "wb");

        if(fp == NULL) {
            char msg[] = "FILE NOT FOUND";
            write(connfd, msg, strlen(msg));
            return;
        }

        // header length before content starts
        long diff = s-buf;

        // write request contents from header to file
        fwrite(s, 1, n-diff, fp);

        // write subsequent socket data to file
        bzero(buf, MAXLINE);
        while((n=recv(connfd,buf,MAXLINE,0)) > 0) {
            fwrite(buf, 1, n, fp);
            bzero(buf, MAXLINE);
        }

        printf("Closing file\n\n");
        fclose(fp);

    }

    if(strncmp(buf, "MKDIR", 5) == 0) {

        s = parse_request(buf, command, subdir, filename, username, password);
        printf("MKDIR %s\r\nUsername: %s\r\nPassword: %s\n\n", filename, username, password);

        filename[strlen(filename)-1] = '\0';
        sprintf(user_directory, ".%s/%s", DIRNAME, username);
        
        struct stat st;
        if (stat(user_directory, &st) == -1) {
            mkdir(user_directory, 0700);
        }

        sprintf(path, "%s/%s", user_directory, filename);
        printf("Making directory: %s\n", path);

        if (stat(path, &st) == -1) {
            mkdir(path, 0700);
        }
    }

    else {
        return;
    }
    
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

