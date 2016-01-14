#ifndef __GPIO_STRUCT_H
#define __GPIO_STRUCT_H
/*gpio�Զ���ṹ��*/


//gpio�����ṹ��
typedef struct _n32926_gpio_arg 
{
	int port;
  int pin;
	int data;//����
	int dire;//����(0���� 1���)
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
