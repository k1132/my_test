#ifndef __GPIO_STRUCT_H
#define __GPIO_STRUCT_H
/*gpio自定义结构体*/


//gpio参数结构体
typedef struct _n32926_gpio_arg 
{
	int port;
  int pin;
	int data;//数据
	int dire;//方向(0输入 1输出)
}GPIO_ARG;

typedef enum 
{
  EM_A = 0,
  EM_B = 1,
  EM_C = 2,
  EM_D = 3,
  EM_E = 4,
  EM_G = 5,
  EM_H = 6,
}EM_PORT_TYPE;

#endif/*__GPIO_STRUCT_H*/
/*******************end************/
