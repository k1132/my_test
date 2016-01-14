#include <linux/module.h> //ģ������Ĵ������źͺ�������
#include <linux/init.h> //ָ����ʼ�����������
#include <linux/fs.h> //�ļ�ϵͳ��صĺ�����ͷ�ļ�
#include <linux/cdev.h> //cdev�ṹ��ͷ�ļ�
#include <asm/uaccess.h> //���ں˺��û��ռ����ƶ����ݵĺ���
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
 * function: Τ������
 * author: lsm
 * date: 2015-12-18
 * ready: wait test
 * watch:
 *  1.weigen�쳣��������:��λ����ʧ
 *  2.��Դ����:ԭ�ӡ�����������д����
 *  3.���֪ͨ�û���:�źš�
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
static int major;//���豸��
int minor = 0; //ָ�����豸��
static WG_INFO s_wgInfo[2] = {0};
struct work_struct weigen_wq;//����һ����������
struct timer_list weigen_timer;//��ʱ��
//DEFINE_TIMER(_name,_function,_expires,_data) ���Զ���ͳ�ʼ����ʱ��
//static struct atomic_t v = ATOMIC_INIT(0);//ԭ�Ӳ���
//wait_queue_t weigen_queue;//�ȴ�����
static DECLARE_WAIT_QUEUE_HEAD(weigen_queue);//�ȴ�����ͷ

static int weigen_drv_init(void);
static irqreturn_t ext_handler(int irq, void *dev_id);
static int weigen_drv_open(struct inode *inode, struct file *file);
static ssize_t weigen_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t weigen_drv_read(struct file *file,  char __user *buf, size_t count, loff_t *ppos);
static long weigen_drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void weigen_drv_exit(void);
static unsigned int weigen_poll(struct file *file, struct poll_table_struct *wait);
static void weigen_do_timer(unsigned long data);//��ʱ����

#ifdef _USE_TASKLET
static void weigen_tasklet_func(unsigned long data);//�װ벿����
#endif
static void weigen_wq_func(unsigned long data);//�װ벿����
bool Parse_WeigenData(unsigned char* pSrcData, unsigned char* pDestData);//����Τ������

static struct file_operations gpio_drv_fops = {
    .owner  =   THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
    .open   =   weigen_drv_open,     
	  .write	=	  weigen_drv_write,	
	  .read   =   weigen_drv_read,
	  .unlocked_ioctl = weigen_drv_ioctl,
	  .poll = weigen_poll,
};

#ifdef _USE_TASKLET
//�װ벿tasklet
DECLARE_TASKLET(weigen_tasklet , weigen_tasklet_func, 0);//������tasklet_struct�ṹweigen_tasklet
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
	unsigned char even_check=0; //żУ��
	unsigned char odd_check=0;	//��У��
  
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
	if((even_check%2) != pSrcData[0]&&(odd_check%2) == pSrcData[WG_LEN-1])//У��
	{
		printk("even_or_odd check err\n");
		return false;
	}
	//ת��
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
      //����
      if(Parse_WeigenData(s_wgInfo[i].wg_bk_data, s_wgInfo[i].wg_card_data))
      {
        s_wgInfo[i].wg_tx_flag = 1;//�ɷ���
        wake_up_interruptible(&weigen_queue);//���ѵȴ�����
      }
    }
  
  }
}

unsigned int weigen_poll(struct file *file, struct poll_table_struct *wait)
{


}


int weigen_drv_init(void)//init ģ��ʱ
{
  int err = 0;

	major = register_chrdev(0, "weigen", &gpio_drv_fops); // ע��, �����ں�
	drv_class = class_create(THIS_MODULE, "weigen_class");
	drv_class_dev = device_create(drv_class, NULL, MKDEV(major, 0), NULL, "WEIGEN"); /* /dev/WEIGEN */
	//����IRQ
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
  //��ʼ����ʱ��
  init_timer(&weigen_timer);
  weigen_timer.function = &weigen_do_timer;
  weigen_timer.data = 0;
  weigen_timer.expires = jiffies + HZ;//1s
  add_timer(&weigen_timer);//��Ӷ�ʱ������
  //�װ벿��������
  INIT_WORK(&weigen_wq, weigen_wq_func);
  return 0;
}

//�ⲿ�ж�4/5����
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

  //���
  if(s_wgInfo[0].wg_count == WG_LEN)
  {
    s_wgInfo[0].wg_count = 0;
    s_wgInfo[0].wg_rx_flag = 1;
    for(i=0; i<WG_LEN; i++)
      s_wgInfo[0].wg_bk_data[i] = s_wgInfo[0].wg_data[i];
#ifdef _USE_TASKLET
    tasklet_schedule(&weigen_tasklet);//���õװ벿����
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
    tasklet_schedule(&weigen_tasklet);//���õװ벿����
#endif
    schedule_work(&weigen_wq);
  }

  if(1 == ret)
    return IRQ_RETVAL(IRQ_HANDLED); 
  else
    return IRQ_RETVAL(IRQ_NONE); 
}

//��ʱ����
void weigen_do_timer(unsigned long data)
{
  //printk("current jiffies%ld\n", jiffies);
  //���¼��붨ʱ
  //weigen_timer.expires = jiffies + HZ;
  //add_timer(&weigen_timer);
  //�޸Ķ�ʱ
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
  //��ѯ�Ĵ���
  reg_h_dir = __raw_readl(REG_GPIOH_DIR);//����Ĵ���
  reg_h_imd = __raw_readl(REG_GPIOH_IMD);//�ж�ģʽ�Ĵ���(��Ե/��ƽ)
  reg_h_iren = __raw_readl(REG_GPIOH_IREN);//����
  reg_h_ifen = __raw_readl(REG_GPIOH_IFEN);//�½�
  reg_h_mfpl = __raw_readl(REG_MFP_GPH_L);//�๦��
  printk("---reg_h_dir=%ld----\n", reg_h_dir);
  printk("---reg_h_imd=%ld----\n", reg_h_imd);
  printk("---reg_h_iren=%ld----\n", reg_h_iren);
  printk("---reg_h_ifen=%ld----\n", reg_h_ifen);
  printk("---reg_h_mfpl=%ld----\n", reg_h_mfpl);
#endif
  
  //disable_irq(IRQ_EXT4);//��ʹ���ж�(�ȴ��жϴ������)
  //enable_irq(IRQ_EXT4);//ʹ���ж�
  //disable_irq(IRQ_EXT4);//��ʹ���ж�(�ȴ��жϴ������)
  //disable_irq_nosync(IRQ_EXT4);//��ʹ���ж�(����)
  //free_irq(IRQ_EXT4, NULL);//�ͷ�
  
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
  unregister_chrdev(major, "weigen"); // ж��
  device_unregister(drv_class_dev);
  class_destroy(drv_class);
}



module_init(weigen_drv_init);
module_exit(weigen_drv_exit);
MODULE_LICENSE("GPL");
/**********************************************end****************************/

