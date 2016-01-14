#include <linux/module.h> //ģ������Ĵ������źͺ�������
#include <linux/init.h> //ָ����ʼ�����������
#include <linux/fs.h> //�ļ�ϵͳ��صĺ�����ͷ�ļ�
#include <linux/cdev.h> //cdev�ṹ��ͷ�ļ�
#include <asm/uaccess.h> //���ں˺��û��ռ����ƶ����ݵĺ���
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/io.h>
#include "w55fa92_reg.h"

#define GPIO_TYPE_NUM     6 //{A,B,C,D,E,G}
#define GPIO_PIN_NUM      16 //0~15
#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETPINUX       _IOW(GPIO_IOC_MAGIC, 0, int)                   
#define IOCTL_GPIO_REVPINMUX      _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE       _IOW(GPIO_IOC_MAGIC, 2, int) 
#define IOCTL_GPIO_GETVALUE    		_IOR(GPIO_IOC_MAGIC, 3, int)

//#define IOCTL_GPIO_SETIRQ    _IOW(GPIO_IOC_MAGIC, 4, int) 
//#define IOCTL_GPIO_CLRIRQ    _IOR(GPIO_IOC_MAGIC, 5, int)

static int gpio_drv_open(struct inode *, struct file *);
static int gpio_drv_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t gpio_drv_write(struct file *, const char *, size_t, loff_t *);
static ssize_t gpio_drv_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

volatile unsigned long *reg_gpiofun[GPIO_TYPE_NUM*2] = {NULL};//gpio���ܼĴ���
volatile unsigned long *reg_gpiocon[GPIO_TYPE_NUM] = {NULL};//gpio���ƼĴ���
volatile unsigned long *reg_gpiodat[GPIO_TYPE_NUM] = {NULL};//gpio������ݼĴ���
volatile unsigned long *reg_gpioval[GPIO_TYPE_NUM] = {NULL};//gpio����ֵ�Ĵ���



//gpio�����ṹ��
typedef struct _n32926_gpio_arg 
{
	int port;
  int pin;
	int data;//����
	int dire;//����
}GPIO_ARG;


static int major;//���豸��
int minor = 0; //ָ�����豸��

static struct class *drv_class;
static struct device	*drv_class_dev;


static struct file_operations gpio_drv_fops = {
    .owner  =   THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
    .open   =   gpio_drv_open,     
	  .write	=	  gpio_drv_write,	
	  .read   =   gpio_drv_read,
	  .ioctl  =   gpio_drv_ioctl,
};


static int gpio_drv_init(void)//init ģ��ʱ
{

  int i;
  printk("init load module \n");  
	major = register_chrdev(0, "my_gpio", &gpio_drv_fops); // ע��, �����ں�
	drv_class = class_create(THIS_MODULE, "gpio_class");
	drv_class_dev = device_create(drv_class, NULL, MKDEV(major, 0), NULL, "GPIO"); /* /dev/GPIO */
#if 1

  //ӳ��gpio���ܼĴ������ڴ�{A,B,C,D,E}
  for(i=0; i<(GPIO_TYPE_NUM-1)*2; i++){
    reg_gpiofun[i] = (volatile unsigned long *)ioremap(REG_GPAFUN0+0x04*i, 32);
  }
  //{G}
  reg_gpiofun[i] = (volatile unsigned long *)ioremap(REG_GPGFUN0, 32);
  reg_gpiofun[++i] = (volatile unsigned long *)ioremap(REG_GPGFUN1, 32);

  //ӳ��gpio���ƼĴ������ڴ�
  for(i=0; i<GPIO_TYPE_NUM; i++){
    reg_gpiocon[i] = (volatile unsigned long *)ioremap(REG_GPIOA_OMD+i*0x10, 32);
  }
  //ӳ��gpio���ݼĴ������ڴ�
  for(i=0; i<GPIO_TYPE_NUM; i++){
    reg_gpiodat[i] = (volatile unsigned long *)ioremap(REG_GPIOA_DOUT+i*0x10, 32);
  }
  //ӳ��gpio����ֵ�Ĵ������ڴ�
  for(i=0; i<GPIO_TYPE_NUM; i++){
    reg_gpioval[i] = (volatile unsigned long *)ioremap(REG_GPIOA_PIN+i*0x10, 32);
  }
#endif  
	return 0;
}

int gpio_drv_open(struct inode *inode, struct file *file)
{
	printk("gpio open\n");

	return 0;
}


int gpio_drv_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
#if 1
  GPIO_ARG gpio_arg; 
  int old_pin;
  if( copy_from_user(&gpio_arg,(GPIO_ARG  __user *)arg, sizeof(GPIO_ARG)) )
  {
    printk("arg invalid!\n");
    return -EFAULT;
  }
  
  //����Ϸ�
  if((gpio_arg.port<0) || (gpio_arg.port>GPIO_TYPE_NUM-1) || 
      (gpio_arg.pin>GPIO_PIN_NUM-1) || (gpio_arg.pin<0) ){
    printk("Pin index invalid!\n");
    return -EINVAL;
  }
  
  switch(cmd)
  {
  case IOCTL_GPIO_SETPINUX://���ùܽ�
    //����Ϊ��ͨ��GPIO
    old_pin = gpio_arg.port ;
    gpio_arg.port = gpio_arg.pin<8? gpio_arg.port*2 : gpio_arg.port*2+1;
    *reg_gpiofun[gpio_arg.port] &= ~(0xF << ((gpio_arg.pin<8? gpio_arg.pin : gpio_arg.pin-8)<<2));//����GPIO
    if(1 == gpio_arg.dire)
      *reg_gpiocon[old_pin] |= (1<<gpio_arg.pin);//���
    else
      *reg_gpiocon[old_pin] &= ~(1<<gpio_arg.pin);//����
    break;
  case IOCTL_GPIO_SETVALUE://����GPIOֵ
    if(1 == gpio_arg.data){
      //�ߵ�ƽ
      *reg_gpiodat[gpio_arg.port] |= (1<<gpio_arg.pin);
    }else {
      //�͵�ƽ
      *reg_gpiodat[gpio_arg.port] &= ~(1<<gpio_arg.pin);
    }
    break;
  case IOCTL_GPIO_GETVALUE://��GPIOֵ
    gpio_arg.data = *reg_gpioval[gpio_arg.port] & (1<<gpio_arg.pin);
    gpio_arg.data = gpio_arg.data? 1 : 0;
    copy_to_user((GPIO_ARG __user *)arg, &gpio_arg, sizeof(GPIO_ARG));
    break;
  default: 
    break;
  }
#endif  
  return 0;
}

ssize_t gpio_drv_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	return 0;
}

ssize_t gpio_drv_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
  //copy_to_user(buf, const void *from, count);
  
  return 0;
}


static void gpio_drv_exit(void)
{

  int i;

  printk("exit upload module \n");
	unregister_chrdev(major, "GPIO"); // ж��
	device_unregister(drv_class_dev);
	class_destroy(drv_class);
	for(i=0; i<GPIO_TYPE_NUM*2; i++){
    iounmap(reg_gpiofun[i]);  
	}
#if 1  	
	for(i=0; i<GPIO_TYPE_NUM; i++){	
    iounmap(reg_gpiocon[i]);
    iounmap(reg_gpiodat[i]);
    iounmap(reg_gpioval[i]);
  }
#endif

}

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);


MODULE_LICENSE("GPL");

