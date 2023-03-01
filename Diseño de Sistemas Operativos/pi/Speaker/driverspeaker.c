#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h> // file operations
#include <asm/uaccess.h> // copy to/from user space
#include <linux/wait.h> // waiting queue
#include <linux/sched.h> // TASK_INTERRUMPIBLE
#include <linux/delay.h> // udelay
#include <linux/interrupt.h>
#include <linux/gpio.h>
#define DRIVER_AUTHOR "DAC"
#define DRIVER_DESC "Ejemplo Driver para placa lab. DAC Rpi"
//GPIOS numbers as in BCM RPi

#define GPIO_SPEAKER 4

/****************************************************************************/
/* LEDs write/read using gpio kernel API */
/****************************************************************************/

/****************************************************************************/
/* LEDs device file operations */
/****************************************************************************/
static ssize_t leds_write(struct file *file, const char __user *buf,
 size_t count, loff_t *ppos)
{
char ch;
if (copy_from_user(&ch,buf,1)) {
	return -EFAULT;
}

printk(KERN_INFO " (write) valor recibido: %d\n",(int)ch);
if(ch == '0'){
	gpio_set_value(GPIO_SPEAKER, 0);
}else{
	gpio_set_value(GPIO_SPEAKER, 1);
}
return 1;
}
/*
if(ch=='1'){
				gpio_set_value(GPIO_SPEAKER, (ch >> 1)& 1);
}
else {
	
		gpio_set_value(GPIO_SPEAKER, (ch>>0)& 0);
	
	}

if (copy_from_user( &ch, buf, 1 )) {
 return -EFAULT;
 }
 
printk( KERN_INFO " valor recibido: %d\n",(int)ch);



return 1;
}

	*/			
			
static const struct file_operations leds_fops = {
 .owner = THIS_MODULE,
 .write = leds_write,
};
/****************************************************************************/
/* SPEAKER device struct */
static struct miscdevice leds_miscdev = {
 .minor = MISC_DYNAMIC_MINOR,
 .name = "speaker",
 .fops = &leds_fops,
};
/*****************************************************************************/
/* This functions registers devices, requests GPIOs and configures interrupts */
/*****************************************************************************/
/*******************************
 * register device for leds
 *******************************/
static void r_dev_config(void)
{
 int ret=0;
 ret = misc_register(&leds_miscdev);
 if (ret < 0) {
 printk(KERN_ERR "misc_register failed\n");
 }
 else
printk(KERN_NOTICE " leds_miscdev.minor =%d\n", leds_miscdev.minor);
}
//return ret;
/*******************************
 * request and init gpios for leds
 *******************************/
static int r_GPIO_config(void)
{
int i;
    int ret=0;
    ret = misc_register(&leds_miscdev);
    if (ret < 0) {
	    printk(KERN_ERR "misc_register gailed\n");
    }else
		printk(KERN_NOTICE "misc_register OK... leds_miscdev.minor=%d\n", leds_miscdev.minor);
		return ret;
}
/*
   
        if ((res=gpio_request_one(GPIO_SPEAKER, GPIOF_INIT_LOW, "HOLA"))) 
        {
            printk(KERN_ERR "GPIO request faiure: led GPIO %d %s\n",GPIO_SPEAKER, "HOLA");
            return res;
        }
        gpio_direction_output(GPIO_SPEAKER,0);
		return res;
	}
*/
//request and init gpios
/*static int r_GPIO_config(void)
{
	int i;
	int res=0;
	res = gpio_request_one(GPIO_SPEAKER, GPIOF_INIT_LOW, "gpio_speaker");
	return res;
}*/
/****************************************************************************/
/* Module init / cleanup block. */
/****************************************************************************/
static int r_init(void) {
printk(KERN_NOTICE "Hello, loading %s module!\n", KBUILD_MODNAME);
 printk(KERN_NOTICE "%s - devices config\n", KBUILD_MODNAME);
 r_dev_config();
 printk(KERN_NOTICE "%s - GPIO config\n", KBUILD_MODNAME);
 r_GPIO_config();
return 0;
}
static void r_cleanup(void) {
 int i;
 printk(KERN_NOTICE "%s module cleaning up...\n", KBUILD_MODNAME);

 
	gpio_set_value(GPIO_SPEAKER, 0);
	gpio_free(GPIO_SPEAKER);
 
 if (leds_miscdev.this_device) misc_deregister(&leds_miscdev);
 printk(KERN_NOTICE "Done. Bye from %s module\n", KBUILD_MODNAME);
return;
}
module_init(r_init);
module_exit(r_cleanup);
/****************************************************************************/
/* Module licensing/description block. */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
