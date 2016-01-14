#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Gpio.h"
/**********测试GPIO**********/
/*
 * 使用例子:
 *  ./test_gpio.out -t A -p 10 -v 0/1(Low/High) -m w
 *  ./test_gpio.out -t A -p 10 -v 0/1(Low/High) -m r
 */

int main(int argc, char* argv[])
{
  int opt;
  int mode;  
  const char *optstring = "t:p:v:m:";//:代表可指定一个值
  CGpio gpio;
  GPIO_ARG gpio_arg;
  
  if(argc < 5){
    printf("arg too less\n");
    return -1;
  }

  while((opt = getopt(argc, argv, optstring)) != -1)  
  {  
    switch(opt)
    {
    case 't':
      if(0 == strcmp("A", optarg)){
        gpio_arg.port = EM_A;
      }else if(0 == strcmp("B", optarg)){
        gpio_arg.port = EM_B;
      }else if(0 == strcmp("C", optarg)){
        gpio_arg.port = EM_C;
      }else if(0 == strcmp("D", optarg)){
        gpio_arg.port = EM_D;
      }else if(0 == strcmp("E", optarg)){
        gpio_arg.port = EM_E;
      }else if(0 == strcmp("G", optarg)){
        gpio_arg.port = EM_G;
      }
      break;
    case 'p':
      gpio_arg.pin = atoi(optarg);
      break;
    case 'v':
      gpio_arg.data = atoi(optarg);
      break;
    case 'm':
      if(0 == strcmp("w", optarg)){
        mode = 1;
      }else if(0 == strcmp("r", optarg)){
        mode = 0;
      }else{
        mode = 0;
      }
      break;
    }
  }  
  
  gpio.Init_Gpio(gpio_arg.port, gpio_arg.pin);
  if(mode == 1){
     gpio.Open_Gpio(1);
    if(1 == gpio_arg.data)
      gpio.Set_Gpio_H();
    else 
      gpio.Set_Gpio_L();
  }else {
    gpio.Open_Gpio(0);
    printf("gpio data=%d\n", gpio.Get_Gpio());
  }
  gpio.Close_Gpio();
  return 0;
}

/*******************************end***********************/
