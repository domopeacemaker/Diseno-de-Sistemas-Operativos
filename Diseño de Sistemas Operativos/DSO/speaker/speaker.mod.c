#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x7d0b00ef, "module_layout" },
	{ 0x3ae715fa, "gpiod_direction_output_raw" },
	{ 0x403f9529, "gpio_request_one" },
	{ 0xb3b172dd, "misc_register" },
	{ 0x3c9b3add, "misc_deregister" },
	{ 0xfe990052, "gpio_free" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xbdb2a4e2, "gpiod_set_raw_value" },
	{ 0xc442bf2a, "gpio_to_desc" },
	{ 0x92997ed8, "_printk" },
	{ 0x12a4e128, "__arch_copy_from_user" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "F7A8020FDC9BB88C4CEF4DC");
