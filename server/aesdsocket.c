#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include	<sys/ioctl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "queue.h"

#define BUFFER_SIZE 100
#define LISTEN_BACKLOG 50
#define SOCKETDATAFILE "/var/tmp/aesdsocketdata"

bool caught_sigint = false;
bool caught_sigterm = false;

int socketfd, acceptedfd;
pthread_mutex_t lock; 

typedef struct client_thread{
   pthread_t thread_id;
   bool threadfinished;
   int sockfd;
   SLIST_ENTRY(client_thread) entries;
} client_thread ;


SLIST_HEAD(thread_list, client_thread);

// FILE *socketdatafile;
static void cleanups();
void *sendreceivesocket(void* arg);
void* log_time(void *arg) ;
static void signal_handler ( int signal_number )
{
    /**
    * Save a copy of errno so we can restore it later.  See https://pubs.opengroup.org/onlinepubs/9699919799/
    * "Operations which obtain the value of errno and operations which assign a value to errno shall be
    *  async-signal-safe, provided that the signal-catching function saves the value of errno upon entry and
    *  restores it before it returns."
    */
    int errno_saved = errno;
    if ( signal_number == SIGINT ) {
        caught_sigint = true;
    } else if ( signal_number == SIGTERM ) {
        caught_sigterm = true;
    }
    errno = errno_saved;
}

static void cleanups(){
   if( caught_sigint || caught_sigterm)
         printf("\nCaught signal, exiting\n");
   printf("Exiting application \n");

   if(shutdown(socketfd,SHUT_RDWR)==-1)
      perror("socketfd shutdown");
   if (close(socketfd) == -1)
      perror("socketfd close");
   // if(shutdown(acceptedfd,SHUT_RDWR)==-1)
   //    perror("acceptfd shutdown");
   if (close(acceptedfd) == -1)
      perror("acceptfd close");
   // if (fclose(socketdatafile) == -1)
   //    perror("socketdatafile close");
   if(remove(SOCKETDATAFILE)!= 0)
      perror("removing socket data file");
    if (pthread_mutex_destroy(&lock) != 0) { 
        perror("mutex destroy"); 
    } 
}
 

int main(int argc, char** argv)
{   
   //setup sigaction
    struct sigaction new_action;
    memset(&new_action,0,sizeof(struct sigaction));
   //  new_action.sa_flags = SA_RESTART;
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

   struct addrinfo hints;
   struct addrinfo *servinfo;
   struct sockaddr_in peer_addr;
   socklen_t peer_addr_size;
   peer_addr_size = sizeof(struct sockaddr_in);
   /* sizeof incoming packet*/
   // char *readbuffer;
   // readbuffer = (char*) malloc(BUFFER_SIZE * sizeof(char));
   openlog("aesdsocket",0,LOG_USER);
   socketfd = socket(PF_INET, SOCK_STREAM, 0);
   if(socketfd == -1){
      perror("socketfd() failed \n");
      exit(EXIT_FAILURE);
   }
   if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
      perror("setsockopt client (SO_REUSEADDR) failed");
      exit(EXIT_FAILURE);
   }
   memset(&hints,0,sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   int s = getaddrinfo(NULL, "9000", &hints, &servinfo);
   if (s != 0) {
      perror("getaddrinfo failed");
      exit(EXIT_FAILURE);
   }
   if (bind(socketfd,servinfo->ai_addr,servinfo->ai_addrlen) == -1){
      perror("bind() client failed \n");
      exit(EXIT_FAILURE);
   } 
   if(listen(socketfd,LISTEN_BACKLOG) == -1){
      perror("listen() client failed \n");
      exit(EXIT_FAILURE);
   }
   if(argc > 1 && strcmp(argv[1],"-d")==0){
      pid_t daemon;

      daemon = fork();
      if(daemon==-1){
         perror("Daemon");
      }else if(daemon != 0){
         printf("Parent Daemon, exiting \n");
         exit(EXIT_SUCCESS);
      }
      if (setsid() == -1) {
         perror("setsid");
         exit(EXIT_FAILURE);
      }
      if(chdir("/") == -1) {
         perror("chdir");
         exit(EXIT_FAILURE);
      }
      int devnull;
      devnull = open("/dev/null", O_WRONLY);
      dup2(devnull,STDIN_FILENO);
      dup2(devnull,STDOUT_FILENO);
      dup2(devnull,STDERR_FILENO);
      printf("starting daemon \n");
   }
   freeaddrinfo(servinfo);

   struct thread_list head;
    SLIST_INIT(&head);

   if (pthread_mutex_init(&lock, NULL) != 0) { 
        perror("mutex init"); 
        exit(EXIT_FAILURE);
    } 
   pthread_t thread_timer;
   pthread_create(&thread_timer, NULL, log_time, NULL);

   while(!(caught_sigint || caught_sigterm)){

      acceptedfd = accept(socketfd,(struct sockaddr *) &peer_addr, &peer_addr_size);
      if(acceptedfd <0){
         if (errno == EINTR && (caught_sigint || caught_sigterm)) break;
            perror("accept() client failed \n");
            continue;
      }
      syslog(LOG_USER,"Accepted connection from %s",inet_ntoa(peer_addr.sin_addr));
      printf("Accepted connection from %s\n", inet_ntoa(peer_addr.sin_addr));

      client_thread *ct = malloc(sizeof(client_thread));
      if (ct == NULL) {
         perror("Failed to allocate memory for thread info");
         close(acceptedfd);
         continue;
      }
      ct->sockfd = acceptedfd;
      ct->threadfinished = false;
      pthread_create(&ct->thread_id, NULL, sendreceivesocket, ct);
      SLIST_INSERT_HEAD(&head, ct, entries);

   }

   client_thread *temp, *ct = SLIST_FIRST(&head);
    while (ct != NULL) {
        if (ct->threadfinished) {
            pthread_join(ct->thread_id, NULL);
            temp = SLIST_NEXT(ct, entries);
            SLIST_REMOVE(&head, ct, client_thread, entries);
            free(ct);
            ct = temp;
        } else {
            ct = SLIST_NEXT(ct, entries);
        }
    }

   syslog(LOG_USER,"Closed connection from %s",inet_ntoa(peer_addr.sin_addr));
   printf("Closed connection from %s\n", inet_ntoa(peer_addr.sin_addr));

   printf("Performing cleanup");
   cleanups();
   return 0;
}


void *sendreceivesocket(void *arg){
   client_thread *ct = (client_thread *)arg;
   char readbuffer[BUFFER_SIZE] = {0};
   FILE* socketdatafile;
   socketdatafile = fopen(SOCKETDATAFILE,"a+");
   if(socketdatafile==NULL){
      perror("fopen");
      // return (void*)-1;
   }
   ssize_t recevBytes, sentBytes;
   while((recevBytes = recv(ct->sockfd, readbuffer, BUFFER_SIZE, 0)) > 0){
      int rc = pthread_mutex_lock(&lock);
      if(rc != 0){
         perror("muxtex lock");
         // return (void*)-1;
      }
      fwrite(readbuffer , 1 , recevBytes , socketdatafile);
      rc = pthread_mutex_unlock(&lock);
      if(rc != 0){
         perror("mutex unlock");
         // return (void*)-1;
      }
      if(recevBytes != BUFFER_SIZE){ //last chunk of bytes in buffer in loop, we can close the file and send
         if (fclose(socketdatafile) == -1){
            perror("socketdatafile close");
            // return (void*)-1;
         }
         socketdatafile = fopen("/var/tmp/aesdsocketdata","r");
         if(socketdatafile==NULL){
            perror("fopen");
            // return (void*)-1;
         }
         while ((sentBytes = fread(readbuffer,sizeof(char),BUFFER_SIZE,socketdatafile)) > 0){
            if(send(ct->sockfd,readbuffer,sentBytes,0) == -1){
               perror("send \n");
            }
         }
      }
   }
   
   if (close(ct->sockfd) == -1){
      perror("ct->sockfd close");
      // return (void*)-1;
   }
   ct->threadfinished = true;
   if (fclose(socketdatafile) == -1){
      perror("socketdatafile close");
      // return (void*)-1;
   }
   return NULL;
}


void* log_time(void *arg) {
    FILE *fp;
    time_t raw_time;
    struct tm *time_info;
    char buffer[80];  // Buffer to hold the formatted time

    while (1) {
        time(&raw_time);
        time_info = localtime(&raw_time);

        strftime(buffer, sizeof(buffer), "timestamp:%Y-%m-%d %H:%M:%S", time_info);

        int rc = pthread_mutex_lock(&lock);
        if(rc != 0){
            perror("muxtex lock");
            // return (void*)-1;
         }
        fp = fopen(SOCKETDATAFILE, "a");
        if (fp != NULL) {
            fprintf(fp, "%s\n", buffer);
            fclose(fp);
        } else {
            perror("Failed to open the log file");
        }
        rc = pthread_mutex_unlock(&lock);
        if(rc != 0){
            perror("muxtex lock");
            // return (void*)-1;
         }

        sleep(10);  // Sleep for 10 seconds
    }

    return NULL;
}