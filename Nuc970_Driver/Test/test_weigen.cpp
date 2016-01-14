#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(void)
{
  int i;
  int fd;
  int val = 1;
  unsigned char card_buf[9];
  
  fd = open("/dev/WEIGEN", O_RDWR);
  if(fd < 0)
  {
    printf("can't open /dev/WEIGEN!\n");
  }

  printf("read weigen \n");
  while(1){
    if(read(fd, card_buf, 9) > 0){
      printf("read card num \n");
      for(i=0; i<9; i++)
        printf("%02x ", card_buf[i]);
      printf("\n");
    }
  }
  return 0;
}


/***************************************end***************************/

