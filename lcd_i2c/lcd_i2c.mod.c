#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

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



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xeb081dc5, "i2c_register_driver" },
	{ 0xe095e43a, "device_destroy" },
	{ 0x4a41ecb3, "class_destroy" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x122c3a7e, "_printk" },
	{ 0x5b08cec9, "i2c_del_driver" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x61f9c02a, "i2c_smbus_write_byte" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xdcb764ad, "memset" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xd75c6742, "__register_chrdev" },
	{ 0xf311fc60, "class_create" },
	{ 0x93ab9e33, "device_create" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("i2c:lcd_i2c");

MODULE_INFO(srcversion, "B433AAC64EFEB503838F81E");
