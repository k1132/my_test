#include <stdio.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <sys/types.h>  
#include <sys/stat.h>  
#include <sys/ioctl.h>  
#include <fcntl.h>  
#include <linux/fs.h>  
#include <errno.h>  
#include <string.h>  
  
int main(void)  
{  
  int fd ;  
  char temp = 1;  
  unsigned int values = 0; 
  int len ;  

  fd = open("/dev/ADC", 0);  
  if (fd < 0)  
  {  
      perror("open ADC device !");  
      exit(1);  
  }  
    
  for( ; ; )  
  {  
    len = read(fd, &values, 4);  
    if (len >= 0)  
    {  
      printf("ADC Value: %d\n", values);  
    }  
    else  
    {  
      perror("read ADC device !");  
      goto  adcstop; 
    }  
    sleep(1);  
  }  
adcstop:      
    close(fd);  
}  

