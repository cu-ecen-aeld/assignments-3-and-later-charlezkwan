#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main( int argc, char *argv[] ) // argc is the (c)ount of arguments, argv is the (v)alues
{
  char *writefile = argv[1];
  char *writestr = argv[2];
  openlog(NULL,0,LOG_USER);
  if(argc != 3){
    printf("Invalid Number of Arguments :%d \n",argc);
    syslog(LOG_ERR,"Invalid Number of Arguments :%d",argc);
    return 1;
  }
  int fd;
  fd = open(writefile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if(fd == -1){
    printf("Create file failed :%s \n",writefile);
    syslog(LOG_ERR,"Create file failed :%s",writefile);
    return 1;
  }else{
    ssize_t nr;
    nr = write(fd,writestr,strlen(writestr));
    if(nr == -1){
        printf("Write file failed :%s \n",writefile);
        syslog(LOG_ERR,"Write file failed :%s",writefile);
        return 1;
    }else{
        syslog(LOG_DEBUG,"Write %s to %s",writestr, writefile);
        if(close(fd)== -1){
            printf("Close file failed :%s \n",writefile);
            syslog(LOG_ERR,"Close file failed :%s",writefile);
            return 1;
        }
        closelog();
        return 0;
    }
  } 
}