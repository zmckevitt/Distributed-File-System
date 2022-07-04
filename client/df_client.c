/* 
 * df_client.c
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
#include <openssl/md5.h> /* requires installation of libssl-dev */
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "linkedlist.h"

#define MAXLINE     8192  /* max text line length */
#define MAXBUF      8192  /* max I/O buffer size */
#define BUFSIZE     1024  /* buf size of 1024 for smaller buffers */
#define NUM_SERVERS 4
#define ENC_KEY     'X'

int open_outfd(char* host, int port) {

    int sockfd;
    struct sockaddr_in serveraddr, cli;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("Socket open failed.\n");    
    bzero(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(host);
    serveraddr.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    struct timeval timeout;      
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                sizeof timeout) < 0)
        return -1;

    if(connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0){
        printf("Connection failed.\n");
        return -1;
    }

    return sockfd;
}

struct dfs_info {
    char addr[24];
    int port;
}; 

// read conf files and determine server IPs and Ports as well as username/password 
int read_conf(char* fname, struct dfs_info* dfs, char* username, char* password) {

    printf("Initializing servers from configuration file... ");

    FILE* fp = fopen(fname, "r");

    if(fp == NULL) {
        printf("No such configuration file");
        return -1;
    }

    char line[1024];
    char* s;
    char* c;
    int i = 0;
    while(fgets(line,1024,fp)) {
        if((s = strstr(line, "Server DFS")) != NULL) {
            s+=12; //increment pointer past "Server DFSX " string
            c = strchr(s, ':');
            strncpy(dfs[i].addr, s, c-s);
            dfs[i].addr[c-s] = '\0';

            s=c+1; //increment string past colon for port number
            dfs[i].port = atoi(s);

        }
        else if((s=strstr(line, "Username: ")) != NULL) {
            s+=10;
            strcpy(username, s);
            if(username[strlen(s) - 1] == '\n')
                username[strlen(s)-1] = '\0';
        }
        else if((s=strstr(line, "Password: ")) != NULL) {
            s+=10;
            strcpy(password, s);

            if(password[strlen(s) - 1] == '\n')
                password[strlen(s)-1] = '\0';
        }
        else {
            break;
        }
        ++i;

    }

    fclose(fp);
    printf("Done!\n\n");
    return 0;
}

// get md5 hash of file and store in c
void md5hash(char* fname, unsigned char* c) {
    
    int bytes;
    FILE* fp = fopen(fname, "rb");

    if(fp == NULL){
        // printf("File does not exist!\n");
        return;
    }

    MD5_CTX mdContext;
    unsigned char data[1024];

    MD5_Init (&mdContext);

    while((bytes=fread(data,1,1024,fp)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c, &mdContext);

}

// Route:
// Array of fixed size 2 * NUM_SERVER where each page gets 2 slots of route data
// Example: md5 % 4 == 0
// Page1 -> DFS1/4, Page2-> DFS1/2, Page3-> DFS2/3, Page4->DFS3/4
// Array = [1,4,1,2,2,3,3,4]

int* route(int index) {
    int* array = malloc(2*NUM_SERVERS*sizeof(int));

    if(index == 0) {
        int temp[] = {1,4,1,2,2,3,3,4};
        memcpy(array, temp, 2*NUM_SERVERS*sizeof(int));
    }
    else if(index == 1) {
        int temp[]  = {1,2,2,3,3,4,4,1};
        memcpy(array, temp, 2*NUM_SERVERS*sizeof(int));

    }
    else if(index == 2) {
        int temp[]  = {2,3,3,4,4,1,1,2};
        memcpy(array, temp, 2*NUM_SERVERS*sizeof(int));
    }
    else if(index == 3) {
        int temp[] = {3,4,4,1,1,2,2,3};
        memcpy(array, temp, 2*NUM_SERVERS*sizeof(int));
    }
    else {
        return NULL;
    }

    return array;

}

// build full file from pieces if possible
// otherwise remove pieces
int assemble(char* filename, int delete) {

    
    char path[1024];
    char cmd[] = "rm -rf temp";
    char newpath[512];
    char buf[MAXLINE];
    int n;

    // if delete flag not set, do not remove existing file
    // flag not set if credentials invalid or subfolder doesnt exist
    if(delete == 0) {
        system(cmd);
        return 0;
    }

    sprintf(newpath, "./get_files/%s", filename);

    // if file already exists, skip assembly
    if(access(newpath, F_OK) == 0) {
        system(cmd);
        return 0;
    }

    // Maybe open for writing first to get blank file?
    FILE* fp = fopen(newpath, "ab");

    for(int i=0; i<NUM_SERVERS; ++i) {

        sprintf(path, "./temp/%s.%d", filename, i+1);

        FILE* fp2 = fopen(path, "rb");

        // Not all pieces present
        if(fp2 == NULL) {
            system(cmd);

            // remove incomplete file from get_files
            sprintf(buf, "rm %s", newpath);
            system(buf);
            return -1;
        }

        while(1) {
            n = fread(buf, 1, MAXLINE, fp2);
            fwrite(buf, 1, n, fp);

            if(n != MAXLINE)
                break;
        }

    }
    system(cmd);
    fclose(fp);
    return 0;
}

void cipher(char* buf) {
    for(int i=0; i<MAXLINE; ++i) {
        buf[i] ^= ENC_KEY;
    }
}

int parse_cmd(char* cmd, char* filename, char* subdir) {

    char* ch;
    char* s;

    if((ch=strchr(cmd, ' ')) != NULL){
        
        ++ch;
        if(strchr(ch, '/') == NULL) {
            printf("Directory requires a trailing \'/\'!\n");
            return -1;
        }

        if(strncmp(ch, "NONE", 4) == 0) {
            printf("Invalid subfolder name!\n");
            return -1;
        }

        s = strtok(cmd, " ");
        strcpy(filename, s);
        strcpy(subdir, ch);

        if(subdir[strlen(subdir)-1] == '\n')
            subdir[strlen(subdir)-1] = '\0';

    } else {
        strcpy(filename, cmd);
        strcpy(subdir, "NONE");
    }

    return 0;
}

int check_err(char* buf) {

    if(strstr(buf, "Invalid credentials.") != NULL) {
        printf("Invalid credentials.\n");
        bzero(buf, MAXLINE);
        return -1;
    }
    if(strstr(buf, "Subfolder does not exist.") != NULL) {
        printf("Subfolder does not exist.\n");
        bzero(buf, MAXLINE);
        return -1;
    }

    return 0;
}

#define DEBUG

int main(int argc, char **argv) {

    if(argc < 2) {
        printf("Usage: ./%s <conf file>", argv[0]);
        return -1;
    }

    // initialize the DFS server information
    char username[128];
    char password[128];
    char buf[MAXBUF];
    char dat[MAXLINE]; 
    char header[1024];
    char header2[1024];
    char filename[300];
    char temp[256];
    char subdir[256];
    char* s;
    int i, j, n;
    int sockfd;
    struct dfs_info servers[4];

    n = read_conf(argv[1], servers, username, password);

    // print server information, make sure conf file read correctly
    #ifdef DEBUG
    printf("Received the following server data: \n");
    for(int i=0; i<4; ++i)
        printf(" - DFS %d IP %s:%d\n", i+1, servers[i].addr, servers[i].port);
    printf("\n");
    printf("Username: %s\n", username);
    printf("Password: %s\n\n", password);
    #endif

    // print list of commands
    printf("---------------Commands---------------\n"
    "  list - show files stored on DFS servers.\n"
    "  get - retrieve a file from DFS servers.\n"
    "  put - upload a file to DFS servers.\n\n");

    while(1) {  

        printf("> ");
        fgets(buf, MAXBUF, stdin);

        // LIST NOT WORKING ON ELRA
        if(strstr(buf, "list")) {

            if(parse_cmd(buf, filename, subdir) < 0)
                continue;


            // create header
            sprintf(header, "list\r\nSubfolder: %s\r\nUsername: %s\r\nPassword: %s\r\n\r\n", subdir, username, password);


            // create head for linked list
            struct Node* head = NULL;

            printf("Received the following files:\n");

            for(i=0; i<NUM_SERVERS; ++i) {

                // open socket 
                sockfd = open_outfd(servers[i].addr, servers[i].port);

                // write header to server
                write(sockfd, header, strlen(header));

                while((n = read(sockfd, buf, MAXLINE)) > 0) {

                    // wrong login info
                    if(strstr(buf, "Invalid credentials.") != NULL) {
                        printf("Invalid credentials.\n");
                        bzero(buf, MAXLINE);
                        close(sockfd);
                        break;
                    }

                    // copy response so it can be tokenized
                    memcpy(dat, buf, MAXLINE);
                    s = strtok(dat, " ");
                    struct Node* file;

                        // get rid of period prefix
                        s+=1; 

                        // grab page from filename extension and convert to int
                        int page = s[strlen(s) - 1] - '0';

                        // get rid of piece extension
                        s[strlen(s)-2] = '\0';

                        // if file not in list, add it 
                        if((file=lookup(head, s)) == NULL) {
                            struct Node* newnode = create_node(s, page);
                            newnode->next = head;
                            head = newnode;
                        }
                        else {
                            file->pages[page-1] = 1;
                        }

                        write(sockfd, "ACK", 4);

                    bzero(buf, MAXLINE);
                }

                close(sockfd);

            }
            print_list(head);

            delete_list(head);

            bzero(buf, MAXLINE);
        }

        // GET
        else if((s=strstr(buf, "get")) != NULL) {

            // get has no args
            if(strlen(buf) < 6){
                printf("Usage: get <file>\n");
                continue;
            }

            // get filename from GET command and store it in s
            s+=4; s[strlen(s)-1] = '\0';

            // if invalid command continue
            if(parse_cmd(s, filename, subdir) < 0)
                continue;


            // array to keep track of which pages we have
            int pages[] = {0,0,0,0};

            // flag to delete file from get_files
            // user error or invalid subfolder set to zero
            int delete = 1;

            // create temp directory to store file pieces
            mkdir("temp", 0700);

            // iterate through each server
            for(i=0; i<NUM_SERVERS; ++i) {

                // in each server, ask for each page
                for(j=0; j<NUM_SERVERS; ++j) { 

                    // if we already have this page, don't ask for it
                    if(pages[j] == 1)
                        continue;

                    sprintf(header, "get %s %d\r\nSubfolder: %s\r\nUsername: %s\r\nPassword: %s\r\n\r\n", filename, j+1, subdir, username, password);

                    sockfd = open_outfd(servers[i].addr, servers[i].port);

                    if(sockfd < 0) {
                        printf("Connection failed.\n");
                        continue;
                    }

                    write(sockfd, header, strlen(header));

                    n=recv(sockfd, buf, MAXLINE, 0);

                    // server does not have desired piece
                    if(strstr(buf, "FILE NOT FOUND") != NULL) {
                        bzero(buf, MAXLINE);
                        close(sockfd);
                        continue;
                    }

                    if(check_err(buf) < 0) {
                        close(sockfd);
                        delete = 0;
                        break;
                    }

                    // create new path for file pieces and store in temp
                    char filename2[1024];
                    sprintf(filename2, "./temp/%s.%d", filename, j+1);
                    FILE* fp = fopen(filename2, "wb");
                    pages[j] = 1;
                    printf("GETTING CHUNK %d...\n", j+1);

                    while(1) {

                        // decrypt incoming data with cipher
                        cipher(buf);

                        // write data to file
                        fwrite(buf, 1, n, fp);
                        n=recv(sockfd, buf, MAXLINE, 0);

                        if(n != MAXLINE)
                            break;
                    }

                    // uncencrypt last chunk and write
                    cipher(buf);
                    fwrite(buf, 1, n, fp);

                    fclose(fp);
                    close(sockfd);
                }
            }


            bzero(buf, MAXLINE);

            // build all file pieces or display error
            n = assemble(filename, delete);
            if(n < 0) 
                printf("Not all pieces found. Unable to assemble file.\n");

        }

        // PUT
        else if((s=strstr(buf, "put")) != NULL) {
            
            // put has no args
            if(strlen(buf) < 6){
                printf("Usage: put <file>\n");
                continue;
            }

            // byte array for MD5 hash value
            unsigned char c[MD5_DIGEST_LENGTH];

            // get filename from PUT command and store it in s
            s+=4; s[strlen(s)-1] = '\0';
            // strcpy(filename, s);

            if(parse_cmd(s, filename, subdir) < 0)
                continue;

            // hash the filename
            md5hash(filename, c);

            // compute MD5 hash mod 4
            int index = c[MD5_DIGEST_LENGTH - 1] % 4;

            struct stat st;
            stat(filename, &st);
            int file_size = st.st_size;

            // get route information based on hash % 4
            int* routes = route(index); // mapping of page#->server
            int sum = 0; // total bytes read in file
            int piece_size = (int)ceil((double)file_size / (double)NUM_SERVERS);
            int limit = piece_size; // threshold for each piece. E.g. piece3 = 75% of file

            
            FILE* fp = fopen(filename, "rb");

            if(fp == NULL) {
                printf("File not found.\n");
                continue;
            }

            // Iterate over each PAGE (1-4)
            // Num pages = Num servers
            for(i=0; i < NUM_SERVERS; ++i) {

                // create header for server 1
                sprintf(header, "put %s %d\r\nSubfolder: %s\r\nUsername: %s\r\nPassword: %s\r\n\r\n", filename, i+1, subdir, username, password);

                // create header for server 2
                sprintf(header2, "put %s %d\r\nSubfolder: %s\r\nUsername: %s\r\nPassword: %s\r\n\r\n", filename, i+1, subdir, username, password);

                // open socket to server 1
                sockfd = open_outfd(servers[routes[2*i]-1].addr, servers[routes[2*i]-1].port);

                // open socket for server 2
                int sockfd2 = open_outfd(servers[routes[2*i+1]-1].addr, servers[routes[2*i+1]-1].port);

                // write header 1 to server 2
                write(sockfd, header, strlen(header));

                // write header 2 to server 1
                write(sockfd2, header2, strlen(header2));

                // check ACK for server 1
                n=recv(sockfd, buf, MAXLINE, 0);
                if(check_err(buf) < 0)
                    break;

                // check ack for server 2
                n=recv(sockfd2, buf, MAXLINE, 0);
                if(check_err(buf) < 0)
                    break;

                // write file contents to server
                while(1) {
                    n = fread(dat, 1, MAXLINE, fp);

                    // keep track of data written to determine when to stop transmitting piece
                    sum += n;

                    // encrypt data on way out
                    cipher(dat);
                    write(sockfd, dat, n);
                    write(sockfd2, dat, n);
                    bzero(dat, MAXLINE);

                    // move on to next piece
                    if (sum > limit)
                        break;

                    // end of file
                    if(n != MAXLINE)
                        break;
                }

                // increment piece 
                limit += piece_size;

                close(sockfd);
                close(sockfd2);
            }
            fclose(fp);
            free(routes);

            bzero(buf, MAXLINE);

        }
        else if((s=strstr(buf, "MKDIR")) != NULL || (s=strstr(buf, "mkdir")) != NULL) {

            s+=6; s[strlen(s)-1] = '\0';
            if(s[strlen(s)-1] != '/'){
                printf("Directory requires a trailing \'/\'!\n");
                continue;
            }

            if(strcmp(s, "NONE/") == 0) {
                printf("Invalid subfolder name!\n");
                continue;
            }

            sprintf(header, "MKDIR %s\r\nUsername: %s\r\nPassword: %s\r\n\r\n", s, username, password);


            for(i=0; i<NUM_SERVERS; ++i) {

                sockfd = open_outfd(servers[i].addr, servers[i].port);

                if(sockfd < 0) {
                    printf("Connection failed.\n");
                    continue;
                }

                write(sockfd, header, strlen(header));

            }

            bzero(buf, MAXLINE);

        }

        // INVALID COMMAND
        else {
            printf("Please enter a valid command.\n");
        }


    }

    return 0;
}
