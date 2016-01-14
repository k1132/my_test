#include <linux/errno.h>  
#include <linux/kernel.h>  
#include <linux/module.h> //模块所需的大量符号和函数定义
#include <linux/init.h> //指定初始化和清除函数
#include <linux/fs.h> //文件系统相关的函数和头文件
#include <linux/cdev.h> //cdev结构的头文件
#include <asm/uaccess.h> //在内核和用户空间中移动数据的函数
#include <mach/map.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>//混杂设备
/*
 * function: ADC驱动
 * author: lsm
 * date: 2015-1-6
 * ready: wait test
 * watch:
 */
/**********************ADC驱动**********************/
#undef DEBUG_ADC
//#define DEBUG_ADC
#ifdef DEBUG_ADC  
#define DPRINTK(x...) {printk(KERN_DEBUG "test_adc: " x);}  
#else  
#define DPRINTK(x...) (void)(0)  
#endif  

#define IRQ_NAME    "ADC"
#define DEVICE_NAME "ADC"   /* 设备节点: /dev/ADC */ 
#define NUC970_ADC_TIMEOUT	(msecs_to_jiffies(1000))

/* nuc970 adc registers offset */
#define CTL     0x00
#define CONF    0x04
#define IER     0x08
#define ISR     0x0C
#define DATA    0x28
#define VBATDATA 0x2C

typedef struct _ADC_DEV
{
  struct clk *adc_clock;//时钟
  struct clk *adc_eclk;
  unsigned char channel;//通道
  struct completion completion;//用于同步
}ADC_DEV;

static void __iomem *base_addr;
static ADC_DEV adc_dev;

//使用混杂设备
#if 0
struct miscdevice  
{	
  int minor;				
  //次设备号，如果设置为MISC_DYNAMIC_MINOR则系统自动分配	
  const char *name;		//设备名	
  const struct file_operations *fops;		//操作函数	
  struct list_head list;	
  struct device *parent;	
  struct device *this_device;
};
#endif


/* ADC中断处理函数 */
static irqreturn_t adc_handler(int irq, void *dev_id)
{	
  struct ADC_DEV *info = (struct ADC_DEV *)dev_id;
  
  if(readl(base_addr+ISR) & (1|(1<<11)))    //check M_F bit
  {
    writel(1|(1<<11), base_addr + ISR); //clear flag
    complete(&adc_dev.completion);
  }

  return IRQ_HANDLED;
}


/*
 * function: open dev call
 */
static int nuc970_adc_open(struct inode *inode, struct file *filp)
{	
  /* 开发板上ADC的通道0 */	
  adc_dev.channel = 0;	//设置ADC的通道	
  DPRINTK( "ADC opened\n");	
  return 0;
}

/*
 * function: close dev call
 */
static int nuc970_adc_release(struct inode *inode, struct file *filp)  
{  
    DPRINTK( "ADC closed\n");  
    return 0;  
}  


/*
 * function: read adc_value
 */
static ssize_t nuc970_adc_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{	
  unsigned int value;	
  unsigned long timeout;

  // enable channel
  writel((readl(base_addr + CONF) & ~(0x7 << 3)) | (adc_dev.channel << 3), base_addr + CONF);
  // enable MST
  writel(readl(base_addr + CTL) | 0x100, base_addr + CTL);
  //等待完成
  //wait_for_completion(&adc_dev.completion);
  timeout = wait_for_completion_interruptible_timeout( \
    &adc_dev.completion, NUC970_ADC_TIMEOUT);
  if(timeout == 0)
    return -ETIMEDOUT;
  //read value
  value = readl(base_addr + VBATDATA);
  DPRINTK("AIN[%d] = 0x%04x\n", adc_dev.channel, value);		
  copy_to_user(buffer, &value, sizeof(value));

  return 0;
}

  
static struct file_operations dev_fops = {  
    .owner = THIS_MODULE,
    .open =  nuc970_adc_open,
    .read =  nuc970_adc_read,
    .release = nuc970_adc_release,
};  

static struct miscdevice misc = {  
    .minor = MISC_DYNAMIC_MINOR,  
    .name = DEVICE_NAME,  
    .fops = &dev_fops,  
};  


/*
 * function: 加载驱动时初始化
 * step:
 *    1.映射adc寄存器
 *    2.使能adc时钟
 *    3.申请Adc中断
 *    4.注册混杂设备
 */
#if 0
static int __init dev_init(void)
{	
  int ret;

  base_addr = ioremap(NUC970_PA_ADC, 0x20);//映射adc寄存器
  if (base_addr == NULL)	{		
    printk(KERN_ERR "failed to remap register block\n");		
    return -ENOMEM;	
  }
  /* find the clock and enable it */	
  adc_dev.adc_eclk = clk_get(NULL, "adc_eclk");
  if (!adc_dev.adc_eclk)	{		
    printk(KERN_ERR "failed to get adc_eclk clock source\n");		
    return -ENOENT;	
  }
  clk_prepare(adc_dev.adc_eclk);
  clk_enable(adc_dev.adc_eclk);
  adc_dev.adc_clock = clk_get(NULL, "adc");	
  if (!adc_dev.adc_clock)	{		
    printk(KERN_ERR "failed to get adc clock source\n");		
    return -ENOENT;	
  }	
  clk_prepare(adc_dev.adc_clock);
  clk_enable(adc_dev.adc_clock);
  clk_set_rate(adc_dev.adc_eclk, 1000000);
  ret = request_irq(IRQ_ADC, adc_handler, IRQF_SHARED, IRQ_NAME, &adc_dev);	
  if(ret)	{		
    iounmap(base_addr);		
    return ret;	
  }	
  init_completion(&adc_dev.completion);
  writel(1, base_addr + CTL); //enable AD_EN
  //writel(3, base_addr + CTL); //enable AD_EN
  writel(1, base_addr + IER); //enable M_IEN
  writel((1<<2)|(3<<6)|(1<<22), base_addr + CONF); //enable NACEN和ADC Reference Select
  
  ret = misc_register(&misc);	
  printk(IRQ_NAME" initialized\n");	
  return ret;
}
#endif
static int __init dev_init(void)
{	
  int ret;

  base_addr = ioremap(NUC970_PA_ADC, 0x20);//映射adc寄存器
  if (base_addr == NULL)	{		
    printk(KERN_ERR "failed to remap register block\n");		
    return -ENOMEM;	
  }
  /* find the clock and enable it */	
  adc_dev.adc_eclk = clk_get(NULL, "adc_eclk");
  if (!adc_dev.adc_eclk)	{		
    printk(KERN_ERR "failed to get adc_eclk clock source\n");		
    return -ENOENT;	
  }
  clk_prepare(adc_dev.adc_eclk);
  clk_enable(adc_dev.adc_eclk);
  adc_dev.adc_clock = clk_get(NULL, "adc");	
  if (!adc_dev.adc_clock)	{		
    printk(KERN_ERR "failed to get adc clock source\n");		
    return -ENOENT;	
  }	
  clk_prepare(adc_dev.adc_clock);
  clk_enable(adc_dev.adc_clock);
  clk_set_rate(adc_dev.adc_eclk, 1000000);
  ret = request_irq(IRQ_ADC, adc_handler, IRQF_SHARED, IRQ_NAME, &adc_dev);	
  if(ret)	{		
    iounmap(base_addr);		
    return ret;	
  }	
  init_completion(&adc_dev.completion);
  writel(3, base_addr + CTL); //enable AD_EN and ADC_CTL_VBGEN
  writel((1<<8), base_addr + CONF); //enable VBATEN
  writel(1, base_addr + IER); //enable M_IEN

  
  ret = misc_register(&misc);	
  printk(IRQ_NAME" initialized\n");	
  return ret;
}

/*
 * function: 卸载驱动
 */
static void __exit dev_exit(void)
{
  free_irq(IRQ_ADC, &adc_dev);
 iounmap(base_addr);
  clk_disable_unprepare(adc_dev.adc_clock);
  clk_disable_unprepare(adc_dev.adc_eclk);
  misc_deregister(&misc);
}

module_init(dev_init);  
module_exit(dev_exit);  
MODULE_LICENSE("GPL");  
MODULE_AUTHOR("lsm");
MODULE_DESCRIPTION("ADC Drivers for NUC970 Defence");  

/***************************end************************/
