#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/uaccess.h>      // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay
#include <linux/jiffies.h>


#include <linux/interrupt.h>
#include <linux/gpio.h>

//Datos del autor del driver
#define DRIVER_AUTHOR "Carlos Baez Recio"
#define DRIVER_DESC   "Bloqueo - Driver para la placa auxiliar RPi"

//Definimos nombres para las GPIOs
#define GPIO_BUTTON1 2
#define GPIO_BUTTON2 3
#define GPIO_SPEAKER 4
#define GPIO_GREEN1  27
#define GPIO_GREEN2  22
#define GPIO_YELLOW1 17
#define GPIO_YELLOW2 11
#define GPIO_RED1    10
#define GPIO_RED2    9

//Declaraciones para el uso de los Leds-------------------------------------------------------------------------
static int LED_GPIOS[]= {GPIO_GREEN1, GPIO_GREEN2, GPIO_YELLOW1, GPIO_YELLOW2, GPIO_RED1, GPIO_RED2};
static char *led_desc[]= {"GPIO_GREEN1","GPIO_GREEN2","GPIO_YELLOW1","GPIO_YELLOW2","GPIO_RED1","GPIO_RED2"};
static int valor_leds=0; //Guarda el antiguo valor de los leds

//Declaracion buffer de pulsasiones-----------------------------------------------------------------------------
static char *buffer_buttons;

//Interrupciones de buttons-------------------------------------------------------------------------------------
static short int irq_BUTTON1    = 0;
static short int irq_BUTTON2	= 0;

//Declaracion de Timers y sus handlers---------------------------------------------------------------------------
static void timer_handler_1(unsigned long); //Declaro Timer 1
static void timer_handler_2(unsigned long); //Declaro Timer 2
DEFINE_TIMER(timer_button1 /*nombre*/, timer_handler_1 /*funcion*/, 0 /*tiempo*/, 0 /*dato*/);
DEFINE_TIMER(timer_button2 /*nombre*/, timer_handler_2 /*funcion*/, 0 /*tiempo*/, 0 /*dato*/);
static unsigned long ticks=0;
//module_param(ticks, int, S_IRUGO);


//Declaracion de tasklets
static void taskleth_button1(unsigned long);
static void taskleth_button2(unsigned long);
DECLARE_TASKLET(tasklet1, taskleth_button1, 0);
DECLARE_TASKLET(tasklet2, taskleth_button2, 0);

//Operaciones con Leds
static void byte2leds(char ch)
{
    int i;
    int val=(int)ch &0x3F; //Guardo los 6 bits menos significativos
	
	if ((ch >> 6) == 0b00){ //Representa el valor binario que escriba
		for(i=0; i<6; i++) gpio_set_value(LED_GPIOS[i], (val >> i) & 1);
		valor_leds=val;
	}
	else if ((ch >> 6) == 0b01){ //Enciende los bits que ponga a 1 ignorando el resto
		
		for (i=0; i<6; i++){
			if ((val >> i) & 1){
			 gpio_set_value(LED_GPIOS[i], (val >> i) & 1);
			}
		}
		valor_leds=val;
	}
	else if ((ch >> 6) == 0b10){ //Apaga los bit que ponga a 1 ignorando el resto
		
		for (i=0; i<6; i++){
			if ((val >> i) & 1){
			 gpio_set_value(LED_GPIOS[i], (val >> i) & 0);
			}
		}
		valor_leds=val;		
	}

}

static char leds2byte(void)
{
    int val;
    char ch;
    int i;
    ch=0;

    for(i=0; i<6; i++)
    {
        val=gpio_get_value(LED_GPIOS[i]);
        ch= ch | (val << i);
    }
    return ch;
}

//---------------------------LEDS-----------------------------------------

static ssize_t leds_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{

    char ch;

    if (copy_from_user( &ch, buf, 1 )) {
        return -EFAULT;
    }

    printk( KERN_INFO " (write) valor recibido: %d\n",(int)ch);

    byte2leds(ch);

    return 1;
}

static ssize_t leds_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    char ch;

    if(*ppos==0) *ppos+=1;
    else return 0;

    ch=leds2byte();

    printk( KERN_INFO " (read) valor entregado: %d\n",(int)ch);


    if(copy_to_user(buf,&ch,1)) return -EFAULT;

    return 1;
}

static const struct file_operations leds_fops = {
    .owner	= THIS_MODULE,
    .write	= leds_write,
    .read	= leds_read,
};

//------------------------------SPEAKER--------------(write-only)-------

static ssize_t speaker_write (struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
	char ch;
	if (copy_from_user (&ch, buf, 1)){
		return -EFAULT;
	}
	
	printk (KERN_INFO "valor recibido %d\n", (int)ch);
	//Funcion que hace que suene????
	
	if (ch=='0'){
		gpio_set_value(GPIO_SPEAKER, 0);
	}
	else{
		gpio_set_value(GPIO_SPEAKER, 1);
	}
	
	return 1;
}

static const struct file_operations speaker_fops = {
    .owner	= THIS_MODULE,
    .write	= speaker_write,
};

//----------------------------------BUTTONS---------(read-only)---------



static ssize_t buttons_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    char ch;

    if(*ppos==0) *ppos+=1;
    else return 0;

    

    printk( KERN_INFO " (read) valor entregado: %d\n",(int)ch);


    if(copy_to_user(buf,buffer_buttons,1)) return -EFAULT;

    return 1;
	
}

static const struct file_operations buttons_fops = {
    .owner	= THIS_MODULE,
    .read	= buttons_read,
};



// declaramos la cola para bloqueo
DECLARE_WAIT_QUEUE_HEAD(espera);
// semaforo para controlar acceso a condicion de bloqueo
DEFINE_SEMAPHORE(semaforo);  // declaración de un semaforo abierto (valor=1)

// condicion de bloqueo
int bloqueo=1;

// text below will be seen in 'cat /proc/interrupt' command
#define GPIO_BUTTON1_DESC           "Boton 1"
#define GPIO_BUTTON2_DESC			"Boton 2"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEVICE_DESC    "Placa lab. DAC button 1"
#define GPIO_BUTTON2_DEVICE_DESC    "Placa lab. DAC button 2"

//----------------------------------------------------------------------
//----------------------MANEJADORES DE INTERRUPCION---------------------
//----------------------------------------------------------------------

//--------------------MANEJADORES BOTONES-------------------------------

static void taskleth_button1(unsigned long data){
	
	down(&semaforo); // capturamos el semáforo
    bloqueo=0; // cambiamos la condición a no bloqueo
    up(&semaforo);
    printk( KERN_INFO " (tasklet b1) despierta procesos\n");
    wake_up(&espera); //despierta a los procesos en espera
}

static void taskleth_button2(unsigned long data){
	    
    down(&semaforo); // capturamos el semáforo
    bloqueo=0; // cambiamos la condición a no bloqueo
    up(&semaforo);
    printk( KERN_INFO " (tasklet b2) despierta procesos\n");
    wake_up(&espera); //despierta a los procesos en espera
}


static irqreturn_t r_irq_handler1(int irq, void *dev_id, struct pt_regs *regs) {

    disable_irq_nosync(irq);
    printk( KERN_INFO "Se produce interrupcion b1\n");
    mod_timer(&timer_button1, jiffies + ticks);
    tasklet_schedule(&tasklet1);
    
    return IRQ_HANDLED;
}



static irqreturn_t r_irq_handler2(int irq, void *dev_id, struct pt_regs *regs) {
 
    disable_irq_nosync(irq);
    printk( KERN_INFO "Se produce interrupcion b2\n");
    mod_timer(&timer_button2, jiffies + ticks);
	tasklet_schedule(&tasklet2);
	
    return IRQ_HANDLED;
}

//--------------------------MANEJADORES TIMERS--------------------------

static void timer_handler_1(unsigned long dato){
     enable_irq(irq_BUTTON1);
     printk( KERN_INFO " IRQ_BUTTON1 enabled\n");
     
}

static void timer_handler_2(unsigned long dato){
     enable_irq(irq_BUTTON2);
     printk( KERN_INFO " IRQ_BUTTON2 enabled\n");
     
}

//----------------------------------------------------------------------
//-----------CONFIGURACION de INTERRUPCIONES BUTTON 1 y 2---------------
//----------------------------------------------------------------------

static int r_int_config_button1(void){
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


static int r_int_config_button2(void){
	int res=0;
	
	if ((res= gpio_request(GPIO_BUTTON2, GPIO_BUTTON2_DESC))) {
		printk (KERN_ERR "GPIO button2 request failed: %s\n", GPIO_BUTTON2_DESC);
		return res;
	}
	
	if ((res= gpio_set_debounce(GPIO_BUTTON2, 200))){
		printk(KERN_ERR "GPIO button2 set_debounce failed: %s, error: %d\n", GPIO_BUTTON2_DESC, res);
		printk(KERN_ERR "errno: 524 => ENOTSUPP, Operation is not supported\n");
	}
	
	if (( irq_BUTTON2 = gpio_to_irq(GPIO_BUTTON2)) < 0) {
		printk(KERN_ERR "GPIO to IRQ mapping failure %s\n", GPIO_BUTTON2_DESC);
		return irq_BUTTON2;
	}
	
	printk(KERN_NOTICE "Mapped int %d for button2 in gpio %d\n", irq_BUTTON2, GPIO_BUTTON2);
	
	if ((res=request_irq(irq_BUTTON2, (irq_handler_t) r_irq_handler2, IRQF_TRIGGER_FALLING, GPIO_BUTTON2_DESC, GPIO_BUTTON2_DEVICE_DESC))){
		printk (KERN_ERR "Irq Request failure\n");
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
    
    // semaforo para acceso a variable-condición bloqueo   
    if (down_interruptible(&semaforo)) return -ERESTARTSYS;
    
    while (bloqueo) // nos tenemos que bloquear?
    { // estamos bloqueados
		up(&semaforo); // release the lock
		printk( KERN_INFO " (read) comienza bloqueo\n");
		if (wait_event_interruptible(espera, !bloqueo))  return -ERESTARTSYS;
		if (down_interruptible(&semaforo)) return -ERESTARTSYS;
	}
	printk( KERN_INFO " (read) fin bloqueo\n");
	bloqueo=1; // vuelve a dejar la condicion de bloqueo a 1
	up(&semaforo);

    if(copy_to_user(buf,respuesta,len)) return -EFAULT;

    return len;
}

//-----------------------Buttons Handlers------------------------------



static const struct file_operations bloqueo_fops = {
    .owner	= THIS_MODULE,
    .read	= b_read,
};



//-------------------ESTRUCTURAS DE DISPOSITIVOS -----------------------                                                    

//Bloqueo device struct
static struct miscdevice b_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "bloqueo",
    .fops	= &bloqueo_fops,
    .mode   = S_IRUGO | S_IWUGO,
};

// LEDs device struct                                                       
static struct miscdevice leds_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "leds",
    .fops	= &leds_fops,
    .mode   = S_IRUGO | S_IWUGO,
};

// Speaker device struct													
static struct miscdevice speaker_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "speaker",
    .fops	= &speaker_fops,
    .mode   = S_IRUGO | S_IWUGO,
};


// Buttons device struct													
static struct miscdevice buttons_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "buttons",
	.fops	= &speaker_fops,
	.mode	= S_IRUGO | S_IWUGO,
};	  


//----------------------------------------------------------------------
//----FUNCIONES PARA EL REGISTRO DE DISPOSITIVOS Y PETICIOS DE GPIOs----
//----------------------------------------------------------------------

//---------------------------REGISTROS----------------------------------

//Registro de dispositivo Bloqueo
static int r_dev_config_bloqueo(void)
{
    int ret=0;
    ret = misc_register(&b_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
    }
	else
		printk(KERN_NOTICE "bloqueo register was successful b_miscdev.minor=%d\n", b_miscdev.minor);
	return ret;
}

//Registro de dispositivo Leds
static int r_dev_config_leds(void){
	int ret=0;
	ret = misc_register(&leds_miscdev);
	if (ret < 0) {
		printk (KERN_ERR "Leds register failed\n");
	}
	else {
		printk(KERN_NOTICE "Leds register was succsessful\n");
	}
	return ret;
}

//Registro de dispositivo Speaker
static int r_dev_config_speaker(void){
	int ret=0;
	ret = misc_register(&speaker_miscdev);
	if (ret < 0) {
		printk(KERN_ERR "Speaker register failed\n");
	}
	else {
		printk(KERN_NOTICE "Speaker register was succsessful\n");
	}
	return ret;
}

//Registro de dispositivo Buttons
static int r_dev_config_buttons(void){
	int ret=0;
	ret = misc_register(&buttons_miscdev);
	
	if (ret < 0){
		printk(KERN_ERR "Buttons register failed\n");
	}
	else{
		printk(KERN_NOTICE "Buttons register was succsesful\n");
	}
	
	return ret;
}


//------------------------------GPIOs-----------------------------------

//GPIOs de los Leds
static int leds_GPIO_config (void){
	int i;
	int res = 0;
	
	for (i=0; i < 6; i++){
		if ((res=gpio_request_one(LED_GPIOS[i], GPIOF_INIT_LOW, led_desc[i]))){
            printk(KERN_ERR "GPIO request faiure: led GPIO %d %s\n",LED_GPIOS[i], led_desc[i]);
            return res;
        }
        gpio_direction_output(LED_GPIOS[i],0);
    }
    return res;
}

//GPIO del Speaker 
static int speaker_GPIO_config (void){
	int res = 0;
	if ((res=gpio_request(GPIO_SPEAKER, "GPIO_SPEAKER"))){
		printk(KERN_ERR "GPIO request failure: speaker GPIO\n");
		return res;
	}
	
	gpio_direction_output(GPIO_SPEAKER, 0);
	
	return res;
}	




//----------------------------------------------------------------------------
//---------------------LIMPIEZA E INICIALIZACION------------------------------                                             
//----------------------------------------------------------------------------

static void r_cleanup(void) {
	int i;
	
    printk(KERN_NOTICE "%s module cleaning up...\n", KBUILD_MODNAME);
    
    //Cleanup bloqueo--------------------------------------------------
    if (b_miscdev.this_device) misc_deregister(&b_miscdev);
    
    
    //Cleanup Leds-----------------------------------------------------
    if (leds_miscdev.this_device) misc_deregister(&leds_miscdev);
    for(i=0; i<6; i++){
	  gpio_free(LED_GPIOS[i]);
	}
    
 
    //Cleanup Speaker--------------------------------------------------
    if (speaker_miscdev.this_device) misc_deregister(&speaker_miscdev);
	gpio_free(GPIO_SPEAKER);


    //cleanup Buttons--------------------------------------------------
    if (buttons_miscdev.this_device) misc_deregister(&buttons_miscdev);
    if (irq_BUTTON1) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);
    if (irq_BUTTON2) free_irq(irq_BUTTON2, GPIO_BUTTON2_DEVICE_DESC);
    gpio_free(GPIO_BUTTON1);
    gpio_free(GPIO_BUTTON2);
    tasklet_kill(&tasklet1);
    tasklet_kill(&tasklet2);
    
    
    //Libera memoria de buffer-----------------------------------------
    vfree(buffer_buttons);
    
    
    printk(KERN_NOTICE "Done. Bye from %s module\n", KBUILD_MODNAME);
    return;
}

static int r_init(void) {
	int res=0;
    printk(KERN_NOTICE "Hello, loading %s module!\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s - devices config...\n", KBUILD_MODNAME);
	
	//Inicializacion bloqueo--------------------------------------------
    if((res = r_dev_config_bloqueo())){
		r_cleanup();
		return res;
	}
	
	
	//Inicializacion Leds-----------------------------------------------
	if((res = r_dev_config_leds())){
		r_cleanup();
		return res;
	}
	
	if ((res = leds_GPIO_config())){
		r_cleanup();
		return res;
	}
	
	//Inicializacion Speaker--------------------------------------------
	if ((res = r_dev_config_speaker())){
		r_cleanup();
		return res;
	}
	
	if ((res= speaker_GPIO_config())){
		r_cleanup();
		return res;
	}

	//Inicializacion Buttons--------------------------------------------
	if ((res = r_dev_config_buttons())){
		r_cleanup();
		return res;
	}
	
	if ((res= r_int_config_button1())){
		r_cleanup();
		return res;
	}

	if ((res= r_int_config_button2())){
		r_cleanup();
		return res;
	}
	
	//Reservamos memoria e inicializamos buffer-------------------------
	buffer_buttons = vmalloc(1024);
	memset(buffer_buttons, 0, 1024);
	
	ticks= msecs_to_jiffies(200);
	
	printk(KERN_NOTICE "%s - Todo inicializado...\n", KBUILD_MODNAME);
	
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

