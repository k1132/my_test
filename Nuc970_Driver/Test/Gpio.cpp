#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include "Gpio.h"

#define INVALID_FILE -1
#define GPIO_L 0
#define GPIO_H 1

/*控制宏*/
#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETPINMUX        _IOW(GPIO_IOC_MAGIC, 0, int)                   
#define IOCTL_GPIO_REVPINMUX        _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE         _IOW(GPIO_IOC_MAGIC, 2, int) 
#define IOCTL_GPIO_GETVALUE         _IOR(GPIO_IOC_MAGIC, 3, int)
#define IOCTL_GPIO_SETIRQ           _IOW(GPIO_IOC_MAGIC, 4, int) 
#define IOCTL_GPIO_CLRIRQ           _IOR(GPIO_IOC_MAGIC, 5, int)

CGpio::CGpio()
{
  memset(&m_param, 0, sizeof(m_param));
  m_fb_gpio = INVALID_FILE;
  strcpy(m_strgpio, "/dev/GPIO");
}

CGpio::~CGpio()
{
  Close_Gpio();
}


/*初始化*/
void CGpio::Init_Gpio(int nport, int npin)
{
  m_param.port = nport;
  m_param.pin = npin;
}

/*打开*/
bool CGpio::Open_Gpio(int nmode)
{
  int ret;
  
  m_fb_gpio = open(m_strgpio, O_RDWR); 
  if (0 > m_fb_gpio){
    printf("open gpio %s faild\n", m_strgpio);
    fprintf(stderr,"Error:%s\n",strerror(errno));
    return false;
  }

  //设置管脚
  m_param.dire = nmode;
  ret = ioctl(m_fb_gpio, IOCTL_GPIO_SETPINMUX, &m_param);
  if(0 != ret){
    printf("set gpio MUX fail\n");
    return false;
  }

  return true;
}


bool CGpio::Close_Gpio(void)
{
  close(m_fb_gpio);
  return true;
}



/*设置低电平*/
void CGpio::Set_Gpio_L(void)
{
  int ret;
  
  m_param.data = GPIO_L;
  ret = ioctl(m_fb_gpio, IOCTL_GPIO_SETVALUE, &m_param);
  if(0 != ret){
    printf("set gpio L fail\n");
  }
}

/*设置高电平*/
void CGpio::Set_Gpio_H(void)
{
  int ret;
  
  m_param.data = GPIO_H;
  ret = ioctl(m_fb_gpio, IOCTL_GPIO_SETVALUE, &m_param);
  if(0 != ret){
    printf("set gpio H fail\n");
  }
}

/*获取gpio的值*/
int CGpio::Get_Gpio(void)
{
  int ret;
  
  ret = ioctl(m_fb_gpio, IOCTL_GPIO_GETVALUE, &m_param);
  if(0 != ret){
    printf("get gpio fail\n");
    return -1;
  }
  
  return m_param.data;
}

/*******************************end*********************/
