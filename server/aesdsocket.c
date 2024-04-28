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

#define BUFFER_SIZE 100
#define LISTEN_BACKLOG 50
#define SOCKETDATAFILE "/var/tmp/aesdsocketdata"

bool caught_sigint = false;
bool caught_sigterm = false;

int socketfd, acceptedfd;
FILE *socketdatafile;
static void exitapp();
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
    exitapp();
}

static void exitapp(){
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
}

int main(int argc, char** argv)
{   

   
   int s;
    struct sigaction new_action;
    memset(&new_action,0,sizeof(struct sigaction));
    new_action.sa_flags = SA_RESTART;
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
    }

   struct addrinfo hints;
   struct addrinfo *servinfo;
   struct sockaddr_in peer_addr;
   socklen_t peer_addr_size;

   char readbuffer[BUFFER_SIZE] = {0};
   /* sizeof incoming packet*/
   // char *readbuffer;
   // readbuffer = (char*) malloc(BUFFER_SIZE * sizeof(char));
   openlog("aesdsocket",0,LOG_USER);

   socketfd = socket(PF_INET, SOCK_STREAM, 0);
   if(socketfd == -1){
      perror("socketfd() failed \n");
      return -1;
   }

   if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
      perror("setsockopt client (SO_REUSEADDR) failed");
      return -1;
   }

   memset(&hints,0,sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;


   s = getaddrinfo(NULL, "9000", &hints, &servinfo);
   if (s != 0) {
      perror("getaddrinfo failed");
      return -1;
   }

   if (bind(socketfd,servinfo->ai_addr,servinfo->ai_addrlen) == -1){
      perror("bind() client failed \n");
      return -1;
   } 
   
   if(listen(socketfd,LISTEN_BACKLOG) == -1){
      perror("listen() client failed \n");
      return -1;
   }
   if(argc > 1 && strcmp(argv[1],"-d")==0){
      pid_t daemon;

      daemon = fork();
      if(daemon==-1){
         perror("Daemon");
      }else if(daemon != 0){
         printf("Parent Daemon, exiting \n");
         return 0;
      }
      if (setsid() == -1) {
         perror("setsid");
         return -1;
      }
      if(chdir("/") == -1) {
         perror("chdir");
      }

      int devnull;
      
      devnull = open("/dev/null", O_WRONLY);
      dup2(devnull,STDIN_FILENO);
      dup2(devnull,STDOUT_FILENO);
      dup2(devnull,STDERR_FILENO);
      
      printf("starting daemon \n");
   }
   freeaddrinfo(servinfo);

   peer_addr_size = sizeof(struct sockaddr_in);

   while(1){
      acceptedfd = accept(socketfd,(struct sockaddr *) &peer_addr, &peer_addr_size);
      if(acceptedfd == -1){
         perror("accept() client failed \n");
         return -1;
      }
      syslog(LOG_USER,"Accepted connection from %s",inet_ntoa(peer_addr.sin_addr));
      printf("Accepted connection from %s\n", inet_ntoa(peer_addr.sin_addr));

      socketdatafile = fopen(SOCKETDATAFILE,"a+");
      if(socketdatafile==NULL){
         perror("fopen");
         return -1;
      }
      ssize_t recevBytes, sentBytes;
      while((recevBytes = recv(acceptedfd, readbuffer, BUFFER_SIZE, 0)) > 0){
         fwrite(readbuffer , 1 , recevBytes , socketdatafile);
         if(recevBytes != BUFFER_SIZE){
            if (fclose(socketdatafile) == -1){
               perror("socketdatafile close");
               return -1;
            }
            socketdatafile = fopen("/var/tmp/aesdsocketdata","r");
            if(socketdatafile==NULL){
               perror("fopen");
               return -1;
            }
            while ((sentBytes = fread(readbuffer,sizeof(char),BUFFER_SIZE,socketdatafile)) > 0){
               if(send(acceptedfd,readbuffer,sentBytes,0) == -1){
                  perror("send \n");
               }
            }
         }
      }
      syslog(LOG_USER,"Closed connection from %s",inet_ntoa(peer_addr.sin_addr));
      printf("Closed connection from %s\n", inet_ntoa(peer_addr.sin_addr));
      if (fclose(socketdatafile) == -1){
         perror("socketdatafile close");
         return -1;
      }

   }



}