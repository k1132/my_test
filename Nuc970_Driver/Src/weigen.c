#include <linux/module.h> //模块所需的大量符号和函数定义
#include <linux/init.h> //指定初始化和清除函数
#include <linux/fs.h> //文件系统相关的函数和头文件
#include <linux/cdev.h> //cdev结构的头文件
#include <asm/uaccess.h> //在内核和用户空间中移动数据的函数
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/io.h>
#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-gpio.h>
#include <mach/regs-gcr.h>
#include <linux/interrupt.h>
/*
 * function: 韦根驱动
 * author: lsm
 * date: 2015-12-18
 * ready: wait test
 * watch:
 *  1.weigen异常出错的情况:移位、丢失
 *  2.资源竞争:原子、自旋锁、读写锁、
 *  3.如何通知用户层:信号、
 *  4.
 */

#define GPIOH4_IRQ IRQ_GPIO_START+0xE4
#define GPIOH5_IRQ IRQ_GPIO_START+0xE5
#define GPIOH6_IRQ IRQ_GPIO_START+0xE6
#define GPIOH7_IRQ IRQ_GPIO_START+0xE7

#define WG_LEN     74
#define CARD_LEN   9

typedef struct _wginfo_
{ 
	unsigned char wg_count;
	unsigned char wg_rx_flag;
  unsigned char wg_tx_flag;
	unsigned char wg_data[WG_LEN];
	unsigned char wg_bk_data[WG_LEN];
	unsigned char wg_card_data[CARD_LEN];
}WG_INFO;

static struct class  *drv_class = NULL;
static struct device *drv_class_dev = NULL;
static int major;//主设备号
int minor = 0; //指定次设备号
static WG_INFO s_wgInfo[2] = {0};
struct work_struct weigen_wq;//定义一个工作队列
struct timer_list weigen_timer;//定时器
//DEFINE_TIMER(_name,_function,_expires,_data) 可以定义和初始化定时器
//static struct atomic_t v = ATOMIC_INIT(0);//原子操作
//wait_queue_t weigen_queue;//等待队列
static DECLARE_WAIT_QUEUE_HEAD(weigen_queue);//等待队列头

static int weigen_drv_init(void);
static irqreturn_t ext_handler(int irq, void *dev_id);
static int weigen_drv_open(struct inode *inode, struct file *file);
static ssize_t weigen_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t weigen_drv_read(struct file *file,  char __user *buf, size_t count, loff_t *ppos);
static long weigen_drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void weigen_drv_exit(void);
static unsigned int weigen_poll(struct file *file, struct poll_table_struct *wait);
static void weigen_do_timer(unsigned long data);//定时处理

#ifdef _USE_TASKLET
static void weigen_tasklet_func(unsigned long data);//底半部处理
#endif
static void weigen_wq_func(unsigned long data);//底半部处理
bool Parse_WeigenData(unsigned char* pSrcData, unsigned char* pDestData);//处理韦根数据

static struct file_operations gpio_drv_fops = {
    .owner  =   THIS_MODULE,    /* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
    .open   =   weigen_drv_open,     
	  .write	=	  weigen_drv_write,	
	  .read   =   weigen_drv_read,
	  .unlocked_ioctl = weigen_drv_ioctl,
	  .poll = weigen_poll,
};

#ifdef _USE_TASKLET
//底半部tasklet
DECLARE_TASKLET(weigen_tasklet , weigen_tasklet_func, 0);//定义了tasklet_struct结构weigen_tasklet
#endif


#ifdef _USE_TASKLET
void weigen_tasklet_func(unsigned long data)
{
  printk("weigen_tasklet_func \n");
  if(1 == s_wgInfo[0].wg_rx_flag)
  {
    s_wgInfo[0].wg_rx_flag = 0;
    printk("weigen recv fin \n");
  }

}
#endif

bool Parse_WeigenData(unsigned char* pSrcData, unsigned char* pDestData)
{
  unsigned char i=0;
	unsigned char j=0;
	unsigned char even_check=0; //偶校验
	unsigned char odd_check=0;	//奇校验
  
	for(i=1; i<WG_LEN-1; i++)
	{
		if(i<37)
		{
			if(pSrcData[i]==0x01)
			{
				even_check++;			
			}
		}
		else
		{
			if(pSrcData[i]==0x01)
			{
				odd_check++;			
			}	
		}	
	}
	if((even_check%2) != pSrcData[0]&&(odd_check%2) == pSrcData[WG_LEN-1])//校验
	{
		printk("even_or_odd check err\n");
		return false;
	}
	//转换
	for(i=0;i<9;i++)
	{
  	pDestData[i] = 0;
		for(j=0;j<8;j++)
		{
			//*(data_buf+i) = *(data_buf+i)<<1;
			//*(data_buf+i) += pData[j+1+8*i];
			*(pDestData+i) += pSrcData[j+1+8*i]<<1*(7-j);
		}
	}
	printk("cardnum=");
	for(i=0; i<9; i++)
	{
  	printk("%02X ", pDestData[i]);
	}
	printk("\n");
	return true;		
}

void weigen_wq_func(unsigned long data)
{
  int i;

  printk("weigen_wq_func \n");
  for(i=0; i<2; i++)
  {
    if(1 == s_wgInfo[i].wg_rx_flag)
    {
      s_wgInfo[i].wg_rx_flag = 0;
      //printk("weigen recv fin \n");
      //处理
      if(Parse_WeigenData(s_wgInfo[i].wg_bk_data, s_wgInfo[i].wg_card_data))
      {
        s_wgInfo[i].wg_tx_flag = 1;//可发送
        wake_up_interruptible(&weigen_queue);//唤醒等待队列
      }
    }
  
  }
}

unsigned int weigen_poll(struct file *file, struct poll_table_struct *wait)
{


}


int weigen_drv_init(void)//init 模块时
{
  int err = 0;

	major = register_chrdev(0, "weigen", &gpio_drv_fops); // 注册, 告诉内核
	drv_class = class_create(THIS_MODULE, "weigen_class");
	drv_class_dev = device_create(drv_class, NULL, MKDEV(major, 0), NULL, "WEIGEN"); /* /dev/WEIGEN */
	//申请IRQ
  err = request_irq(GPIOH4_IRQ, ext_handler, IRQF_SHARED|IRQF_TRIGGER_RISING, "ext4", 4);
  if(err){
    printk(KERN_ERR "IRQ_EXT4 request error %d\n", err);
    return 0;
  }
  err = request_irq(GPIOH5_IRQ, ext_handler, IRQF_SHARED|IRQF_TRIGGER_RISING, "ext5", 5);
  if(err){
    printk(KERN_ERR "IRQ_EXT5 request error %d\n", err);
    return 0;
  }
  err = request_irq(GPIOH6_IRQ, ext_handler, IRQF_SHARED|IRQF_TRIGGER_RISING, "ext6", 6);
  if(err){
    printk(KERN_ERR "IRQ_EXT6 request error %d\n", err);
    return 0;
  }
  err = request_irq(GPIOH7_IRQ, ext_handler, IRQF_SHARED|IRQF_TRIGGER_RISING, "ext7", 7);
  if(err){
    printk(KERN_ERR "IRQ_EXT7 request error %d\n", err);
    return 0;
  }
  //初始化定时器
  init_timer(&weigen_timer);
  weigen_timer.function = &weigen_do_timer;
  weigen_timer.data = 0;
  weigen_timer.expires = jiffies + HZ;//1s
  add_timer(&weigen_timer);//添加定时器队列
  //底半部工作队列
  INIT_WORK(&weigen_wq, weigen_wq_func);
  return 0;
}

//外部中断4/5处理
irqreturn_t ext_handler(int irq, void *dev_id)
{
  int ret = 0;
  int i;
  if(irq == GPIOH4_IRQ){//wg1_d0
    //printk("Ext4 Occur\n");
    s_wgInfo[0].wg_data[s_wgInfo[0].wg_count] = 0x00;
    s_wgInfo[0].wg_count++;
    ret = 1;
  }else if(irq == GPIOH5_IRQ){//wg1_d1
    //printk("Ext5 Occur\n");
    s_wgInfo[0].wg_data[s_wgInfo[0].wg_count] = 0x01;
    s_wgInfo[0].wg_count++;
    ret = 1;
  }else if(irq == GPIOH6_IRQ){//wg2_d0
    //printk("Ext6 Occur\n");
    s_wgInfo[1].wg_data[s_wgInfo[1].wg_count] = 0x00;
    s_wgInfo[1].wg_count++;
    ret = 1;
  }else if(irq == GPIOH7_IRQ){//wg2_d1
    //printk("Ext7 Occur\n");
    s_wgInfo[1].wg_data[s_wgInfo[1].wg_count] = 0x01;
    s_wgInfo[1].wg_count++;
    ret = 1;
  }

  //检测
  if(s_wgInfo[0].wg_count == WG_LEN)
  {
    s_wgInfo[0].wg_count = 0;
    s_wgInfo[0].wg_rx_flag = 1;
    for(i=0; i<WG_LEN; i++)
      s_wgInfo[0].wg_bk_data[i] = s_wgInfo[0].wg_data[i];
#ifdef _USE_TASKLET
    tasklet_schedule(&weigen_tasklet);//调用底半部处理
#endif
    schedule_work(&weigen_wq);
  }
  if(s_wgInfo[1].wg_count == WG_LEN)
  {
    s_wgInfo[1].wg_count = 0;
    s_wgInfo[1].wg_rx_flag = 1;
    for(i=0; i<WG_LEN; i++)
      s_wgInfo[1].wg_bk_data[i] = s_wgInfo[1].wg_data[i];
#ifdef _USE_TASKLET
    tasklet_schedule(&weigen_tasklet);//调用底半部处理
#endif
    schedule_work(&weigen_wq);
  }

  if(1 == ret)
    return IRQ_RETVAL(IRQ_HANDLED); 
  else
    return IRQ_RETVAL(IRQ_NONE); 
}

//定时处理
void weigen_do_timer(unsigned long data)
{
  //printk("current jiffies%ld\n", jiffies);
  //重新加入定时
  //weigen_timer.expires = jiffies + HZ;
  //add_timer(&weigen_timer);
  //修改定时
  mod_timer(&weigen_timer, jiffies + HZ);
  
}

int weigen_drv_open(struct inode *inode, struct file *file)
{
#if 0
  unsigned int reg_h_dir;
  unsigned int reg_h_imd;
  unsigned int reg_h_iren;
  unsigned int reg_h_ifen;
  unsigned int reg_h_mfpl;
  //查询寄存器
  reg_h_dir = __raw_readl(REG_GPIOH_DIR);//方向寄存器
  reg_h_imd = __raw_readl(REG_GPIOH_IMD);//中断模式寄存器(边缘/电平)
  reg_h_iren = __raw_readl(REG_GPIOH_IREN);//上升
  reg_h_ifen = __raw_readl(REG_GPIOH_IFEN);//下降
  reg_h_mfpl = __raw_readl(REG_MFP_GPH_L);//多功能
  printk("---reg_h_dir=%ld----\n", reg_h_dir);
  printk("---reg_h_imd=%ld----\n", reg_h_imd);
  printk("---reg_h_iren=%ld----\n", reg_h_iren);
  printk("---reg_h_ifen=%ld----\n", reg_h_ifen);
  printk("---reg_h_mfpl=%ld----\n", reg_h_mfpl);
#endif
  
  //disable_irq(IRQ_EXT4);//不使能中断(等待中断处理结束)
  //enable_irq(IRQ_EXT4);//使能中断
  //disable_irq(IRQ_EXT4);//不使能中断(等待中断处理结束)
  //disable_irq_nosync(IRQ_EXT4);//不使能中断(立即)
  //free_irq(IRQ_EXT4, NULL);//释放
  
  return 0;
}


ssize_t weigen_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
  return 0;
}

ssize_t weigen_drv_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
  wait_event_interruptible(weigen_queue, s_wgInfo[0].wg_tx_flag || s_wgInfo[1].wg_tx_flag);
  if(1 == s_wgInfo[0].wg_tx_flag){
    s_wgInfo[0].wg_tx_flag = 0;
    copy_to_user(buf, s_wgInfo[0].wg_card_data, 9);
    return 9;
  }
  if(1 == s_wgInfo[1].wg_tx_flag){
    s_wgInfo[1].wg_tx_flag = 0;
    copy_to_user(buf, s_wgInfo[1].wg_card_data, 9);
    return 9;
  }
  return -1;
}

long weigen_drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  
  return 0;
}

void weigen_drv_exit(void)
{
  printk("exit weigen module \n");

  del_timer_sync(&weigen_timer);
  free_irq(GPIOH7_IRQ, 7);
  free_irq(GPIOH6_IRQ, 6);
  free_irq(GPIOH5_IRQ, 5);
  free_irq(GPIOH4_IRQ, 4);
  unregister_chrdev(major, "weigen"); // 卸载
  device_unregister(drv_class_dev);
  class_destroy(drv_class);
}



module_init(weigen_drv_init);
module_exit(weigen_drv_exit);
MODULE_LICENSE("GPL");
/**********************************************end****************************/

