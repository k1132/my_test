#ifndef __GPIO_H
#define __GPIO_H
#include "Gpio_Struct.h"

#define NAMEPATH_LEN 20

class CGpio
{
public:
  CGpio();
  ~CGpio();
public:
  void Init_Gpio(int nport, int npin);
  bool Open_Gpio(int nmode);
  bool Close_Gpio(void);
  void Set_Gpio_L(void);
  void Set_Gpio_H(void);
  int  Get_Gpio(void);
  //�����ⲿ�жϷ�ʽ
  void Set_Gpio_Irq(void);
  void Clean_Gpio_Irq(void);
  
private:
  int  m_fb_gpio;//�豸����
  char m_strgpio[NAMEPATH_LEN];//gpio�豸��
  GPIO_ARG m_param;
};

#endif/*__GPIO_H*/
/**********************end********************/

