#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/cdev.h>
#include <linux/fs.h> 

#define POWCTL_MAJOR		0
#define POWCTL_PRFX		"powctl - "

#define RESOLUTION_60HZ	32680	/* resolution for 8 bits */
#define RESOLUTION_DAC		255	/* 8-bit dac */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gpcm00");
MODULE_DESCRIPTION("AC Power control device driver");

int powctl_major = POWCTL_MAJOR;
int powctl_minor = 0;

struct powctl_dev {
	struct cdev cdev;
	struct hrtimer tmr;
	struct gpio_desc *pw;
	struct gpio_desc *zc;
	int gpio_irq;
	u8 duty_cycle;
} powctl_device;

static int powctl_probe(struct platform_device *pdev);
static int powctl_remov(struct platform_device *pdev);

static struct of_device_id powctl_ids[] = {
	{ .compatible = "powercontrol,gpcm00",},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, powctl_ids);

static struct platform_driver powctl_driver = {
	.probe = powctl_probe,
	.remove = powctl_remov,
	.driver = {
		.name = "power_modulator_driver",
		.of_match_table = powctl_ids,
	},
};

irqreturn_t zero_crossing_irq(int irq_no, void *dev_id)
{
	unsigned long t;
	
	switch(powctl_device.duty_cycle) {
		case 0:
			gpiod_set_value(powctl_device.pw, 0);
			break;
		case RESOLUTION_DAC:
			gpiod_set_value(powctl_device.pw, 1);
			break;
		default:
			gpiod_set_value(powctl_device.pw, 0);
			t = RESOLUTION_60HZ * (RESOLUTION_DAC - powctl_device.duty_cycle);
			hrtimer_start(&powctl_device.tmr, ktime_set(0, t),
			 					HRTIMER_MODE_REL_HARD);
			break;
	}
	
	return IRQ_HANDLED;
}

static enum hrtimer_restart power_control_irq(struct hrtimer *timer)
{
	gpiod_set_value(powctl_device.pw, 1);
	return HRTIMER_NORESTART;
}

static int powctl_probe(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	int ret = -1;

	pr_info(POWCTL_PRFX "Initializing power modulator devices");
	
	/* initialize gpio */
	powctl_device.zc = gpiod_get(dev, "zc", GPIOD_IN);
	if(IS_ERR(powctl_device.zc)) {
		pr_err(POWCTL_PRFX "Error: Could not set up ZC GPIO");
		ret = -IS_ERR(powctl_device.zc);
		goto return_error;
	} 
	
	powctl_device.pw = gpiod_get(dev, "pw", GPIOD_OUT_HIGH);
	if(IS_ERR(powctl_device.pw)) {
		pr_err(POWCTL_PRFX "Error: Could not set up PW GPIO");
		ret = -IS_ERR(powctl_device.pw);
		goto clear_zc;
	}

	gpiod_set_value(powctl_device.pw, 0);

	powctl_device.gpio_irq = gpiod_to_irq(powctl_device.zc);
	if(powctl_device.gpio_irq < 0) {
		pr_err(POWCTL_PRFX "Error: Failed to get an IRQ number");
		goto clear_pw;
	}
	
	if(request_irq(powctl_device.gpio_irq, zero_crossing_irq, 
				IRQF_TRIGGER_RISING, "powctl", NULL) < 0) {
		pr_err(POWCTL_PRFX "Failed to request IRQ");
		goto clear_irq;
	}

	/* initialize timer */
	hrtimer_init(&powctl_device.tmr, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	powctl_device.tmr.function = power_control_irq;

	return 0;

clear_irq:
	free_irq(powctl_device.gpio_irq, NULL);
clear_pw:
	gpiod_put(powctl_device.pw);
clear_zc:
	gpiod_put(powctl_device.zc);
return_error:
	return ret;
}

static int powctl_remov(struct platform_device *pdev) 
{
	hrtimer_cancel(&powctl_device.tmr);
	free_irq(powctl_device.gpio_irq, NULL);
	gpiod_put(powctl_device.pw);
	gpiod_put(powctl_device.zc);
	
	return 0;
}

static ssize_t powctl_read(struct file *filp, char __user *buf, size_t count, 
									loff_t *f_pos)
{
	size_t retval;
	u8 data;
	
	data = powctl_device.duty_cycle;
	if(copy_to_user(buf, &data, 1)) {
        	retval = -EFAULT;
	} else {
		retval = 1;
	}
	
	return retval;

}

static ssize_t powctl_write(struct file *filp, const char __user *buf, size_t count, 
										loff_t *f_pos)
{
	size_t retval;
	u8 data;
	
	if(copy_from_user(&data, buf, 1)) {
		retval = -EFAULT;
	} else {
		retval = 1;
		powctl_device.duty_cycle = data;
	}
	
	return retval;
}


struct file_operations powctl_fops = {
	.owner =            THIS_MODULE,
	.read =             powctl_read,
	.write =            powctl_write,
};

static int __init powctl_init_module(void)
{
	dev_t dev = 0;
	int result;
	
	pr_info(POWCTL_PRFX "Initializing power modulator module");
	
	result = alloc_chrdev_region(&dev, powctl_minor, 1, "powctrl");
	powctl_major = MAJOR(dev);
	if(result < 0) {
		pr_err(POWCTL_PRFX "Error code %d can't get major %d", result, powctl_major);
		return result;
	}
	
	memset(&powctl_device, 0, sizeof(struct powctl_dev));
	cdev_init(&powctl_device.cdev, &powctl_fops);
	powctl_device.cdev.owner = THIS_MODULE;
	
	result = cdev_add(&powctl_device.cdev, dev, 1);
	if(result < 0) {
		pr_err(POWCTL_PRFX "Error code %d adding powctl device", result);
		goto clean_chrdev_region;
	}
	
	result = platform_driver_register(&powctl_driver);
	if(result < 0) {
		pr_err(POWCTL_PRFX "Error code %d registering powctl platform driver", result);
		goto clean_chrdev_region;
	}
	
	return 0;
	
clean_chrdev_region:
	unregister_chrdev_region(dev, 1);
	return result;
	
}

static void __exit powctl_exit_module(void)
{
	dev_t dev = MKDEV(powctl_major, powctl_minor);
	
	pr_info(POWCTL_PRFX "Removing power modulator module\n");
	
	platform_driver_unregister(&powctl_driver);
	unregister_chrdev_region(dev, 1);
}

module_init(powctl_init_module);
module_exit(powctl_exit_module);
