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

#define DRIVER_AUTHOR "Andres Rodriguez - DAC"
#define DRIVER_DESC   "Ejemplo uso cola de bloqueo"

//GPIOS numbers as in BCM RPi

#define GPIO_BUTTON1 2

// declaramos la cola para bloqueo
DECLARE_WAIT_QUEUE_HEAD(espera);
// semaforo para controlar acceso a condición de bloqueo
DEFINE_SEMAPHORE(semaforo);  // declaración de un semáforo abierto (valor=1)

// condición de bloqueo
int bloqueo=1;

/****************************************************************************/
/* Interrupts variables block                                               */
/****************************************************************************/
static short int irq_BUTTON1    = 0;

// text below will be seen in 'cat /proc/interrupt' command
#define GPIO_BUTTON1_DESC           "Boton 1"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEVICE_DESC    "Placa lab. DAC"

/****************************************************************************/
/* IRQ handler - fired on interrupt                                         */
/****************************************************************************/
static irqreturn_t r_irq_handler1(int irq, void *dev_id, struct pt_regs *regs) {

    // todo esto debería ir en un tasklet para no hacer esperar a la interrupción
    down(&semaforo); // capturamos el semáforo
    bloqueo=0; // cambiamos la condición a no bloqeo
    up(&semaforo);
    printk( KERN_INFO " (interrupcion) despierta procesos\n");
    wake_up(&espera); //despierta a los procesos en espera
 
    return IRQ_HANDLED;
}
/*******************************
 *  set interrup for button 1
 *******************************/

static int r_int_config(void)
{
	int res=0;
    if ((res=gpio_request(GPIO_BUTTON1, GPIO_BUTTON1_DESC))) {
        printk(KERN_ERR "GPIO request faiure: %s\n", GPIO_BUTTON1_DESC);
        return res;
    }
    
    if ((res=gpio_set_debounce(GPIO_BUTTON1, 200))) {
        printk(KERN_ERR "GPIO set_debounce failure: %s, error: %d\n", GPIO_BUTTON1_DESC, res);
        printk(KERN_ERR "errno: 524 => ENOTSUPP, Operation is not supported\n");
    }

    if ( (irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0 ) {
        printk(KERN_ERR "GPIO to IRQ mapping faiure %s\n", GPIO_BUTTON1_DESC);
        return irq_BUTTON1;
    }

    printk(KERN_NOTICE "  Mapped int %d for button1 in gpio %d\n", irq_BUTTON1, GPIO_BUTTON1);

    if ((res=request_irq(irq_BUTTON1,
                    (irq_handler_t ) r_irq_handler1,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_DESC,
                    GPIO_BUTTON1_DEVICE_DESC))) {
        printk(KERN_ERR "Irq Request failure\n");
        return res;
    }


    return res;
}

/****************************************************************************/
/* device file operations                                              */
/****************************************************************************/

static ssize_t b_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    char *respuesta="OK\n";
    int len;
    
    len=(count<3)? count : 3; 

    if(*ppos==0) *ppos+=len; else return 0;
    
    // semáforo para acceso a variable-condición bloqueo   
    if (down_interruptible(&semaforo)) return -ERESTARTSYS;
    
    while (bloqueo) // nos tenemos que bloquear?
    { // estamos bloqueados
		up(&semaforo); /* release the lock */
		printk( KERN_INFO " (read) comienza bloqueo\n");
		if (wait_event_interruptible(espera, !bloqueo))  return -ERESTARTSYS;
		if (down_interruptible(&semaforo)) return -ERESTARTSYS;
	}
	printk( KERN_INFO " (read) fin bloqueo\n");
	bloqueo=1; // vuelve a dejar la condición de bloqueo a 1
	up(&semaforo);

    if(copy_to_user(buf,respuesta,len)) return -EFAULT;

    return len;
}

static const struct file_operations b_fops = {
    .owner	= THIS_MODULE,
    .read	= b_read,
};

/****************************************************************************/
/* device struct                                                       */

static struct miscdevice b_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "bloqueo",
    .fops	= &b_fops,
    .mode    = S_IRUGO | S_IWUGO,
};

/*****************************************************************************/
/* This functions registers devices, requests GPIOs and configures interrupts */
/*****************************************************************************/

/*******************************
 *  register device 
 *******************************/

static int r_dev_config(void)
{
    int ret=0;
    ret = misc_register(&b_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
    }
	else
		printk(KERN_NOTICE "misc_register OK... b_miscdev.minor=%d\n", b_miscdev.minor);
	return ret;
}

/****************************************************************************/
/* Module init / cleanup block.                                             */
/****************************************************************************/

static void r_cleanup(void) {
    printk(KERN_NOTICE "%s module cleaning up...\n", KBUILD_MODNAME);
    if (b_miscdev.this_device) misc_deregister(&b_miscdev);
    
    if(irq_BUTTON1) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);   //libera irq
    gpio_free(GPIO_BUTTON1);  // libera GPIO
    
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
	
	printk(KERN_NOTICE "%s - INT config...\n", KBUILD_MODNAME);
  
	if((res = r_int_config()))
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
