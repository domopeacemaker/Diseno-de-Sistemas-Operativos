#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/uaccess.h>      // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay

#include <linux/interrupt.h>
#include <linux/gpio.h>

#define DRIVER_AUTHOR "Juan Álvaro Caravaca Seliva"
#define DRIVER_DESC   "Drivers for Berryclip leds"
#define GPIO_SPEAKER 4



/****************************************************************************/
/* LEDs device file operations                                              */
/****************************************************************************/




static ssize_t speaker_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{

    char ch;

    if (copy_from_user(&ch, buf, 1 )) {
        return -EFAULT;
    }

    printk( KERN_INFO " (write) valor recibido: %d\n",(int)ch);

    //byte2speaker(ch);
    if(ch=='0'){
        gpio_set_value(GPIO_SPEAKER,0);
    }else{
        gpio_set_value(GPIO_SPEAKER,1);
    }

    return 1;
}


static const struct file_operations speaker_fops = {
    .owner	= THIS_MODULE,
    .write	= speaker_write,
    //.read	= leds_read,
};

/****************************************************************************/
/* LEDs device struct                                                       */

static struct miscdevice speaker_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "speaker",
    .fops	= &speaker_fops,
    .mode       = S_IRUGO | S_IWUGO,
};

/*****************************************************************************/
/* This functions registers devices, requests GPIOs and configures interrupts */
/*****************************************************************************/

/*******************************
 *  register device for leds
 *******************************/

static int r_dev_config(void){
    int ret=0;
    ret = misc_register(&speaker_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
    }
	else
		printk(KERN_NOTICE "misc_register OK... speaker_miscdev.minor=%d\n", speaker_miscdev.minor);
	return ret;
}

/*******************************
 *  request and init gpios for leds
 *******************************/

static int r_GPIO_config(void)
{
    
    int res=0;
    
        if ((res=gpio_request_one(GPIO_SPEAKER, GPIOF_INIT_LOW, "speaker"))){
            printk(KERN_ERR "GPIO request faiure: SPEAKER GPIO %d %s\n",GPIO_SPEAKER, "speaker");
            return res;
        }
        gpio_direction_output(GPIO_SPEAKER,1);
	
	return res;
}



/****************************************************************************/
/**********************Module init / cleanup block.**************************/
/****************************************************************************/ 



static void r_cleanup(void) {
    
    printk(KERN_NOTICE "%s module cleaning up...\n", KBUILD_MODNAME);
    gpio_free(GPIO_SPEAKER);
    if (speaker_miscdev.this_device) misc_deregister(&speaker_miscdev);
    printk(KERN_NOTICE "Done. Bye from %s module\n", KBUILD_MODNAME);
    return;
}



static int r_init(void) {

	int res=0;
    printk(KERN_NOTICE "Hello, loading %s module!\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s - devices config...\n", KBUILD_MODNAME);

    if((res = r_dev_config()))
    {
		r_cleanup();
		return res;
	}
    printk(KERN_NOTICE "%s - GPIO config...\n", KBUILD_MODNAME);    
    if((res = r_GPIO_config()))
    {
		r_cleanup();
		return res;
	}
    return res;
}

module_init(r_init);
module_exit(r_cleanup);
/****************************************************************************/
/* Module licensing/description block.                                      */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
