#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>

#define DEVICE_NAME  "lcd_i2c"
#define CLASS_NAME   "lcd_class"

static struct i2c_client *lcd_client;
static int              major_num;
static struct class    *lcd_class;
static struct device   *lcd_device;

static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) | 0x08 | (rs ? 0x01 : 0x00);
    i2c_smbus_write_byte(lcd_client, data | 0x04);  // EN=1
    udelay(50);
    i2c_smbus_write_byte(lcd_client, data & ~0x04); // EN=0
    udelay(50);
}

static void lcd_send_byte(uint8_t byte, uint8_t rs)
{
    lcd_send_nibble(byte >> 4, rs);
    lcd_send_nibble(byte & 0x0F, rs);
    mdelay(2);
}

static void lcd_clear(void)
{
    lcd_send_byte(0x01, 0);
    mdelay(2);
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t addr = row ? (0x40 + col) : col;
    lcd_send_byte(0x80 | addr, 0);
}

static void lcd_print(const char *str)
{
    while (*str)
        lcd_send_byte(*str++, 1);
}

static void lcd_init_display(void)
{
    mdelay(50);
    lcd_send_nibble(0x03, 0); mdelay(5);
    lcd_send_nibble(0x03, 0); mdelay(5);
    lcd_send_nibble(0x03, 0); mdelay(5);
    lcd_send_nibble(0x02, 0);  // 4-bit 모드 진입

    lcd_send_byte(0x28, 0);    // 함수 설정: 4비트, 2라인
    lcd_send_byte(0x0C, 0);    // 디스플레이 ON, 커서 OFF
    lcd_send_byte(0x06, 0);    // 엔트리 모드 설정
    lcd_clear();
}

static ssize_t lcd_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    char kbuf[33];
    size_t to_copy = min(len, (size_t)32);

    if (copy_from_user(kbuf, buf, to_copy))
        return -EFAULT;
    kbuf[to_copy] = '\0';

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print(kbuf);

    return to_copy;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
};

static int lcd_probe(struct i2c_client *client)
{
    lcd_client = client;
    lcd_init_display();

    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0)
        return major_num;

    lcd_class = class_create(CLASS_NAME);
    if (IS_ERR(lcd_class)) {
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(lcd_class);
    }

    lcd_device = device_create(lcd_class, NULL,
                               MKDEV(major_num, 0),
                               NULL, DEVICE_NAME);
    if (IS_ERR(lcd_device)) {
        class_destroy(lcd_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(lcd_device);
    }

    pr_info("lcd_i2c: LCD initialized (major=%d)\n", major_num);
    return 0;
}

static void lcd_remove(struct i2c_client *client)
{
    device_destroy(lcd_class, MKDEV(major_num, 0));
    class_destroy(lcd_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    pr_info("lcd_i2c: LCD removed\n");
}

static const struct i2c_device_id lcd_id[] = {
    { "lcd_i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_id);

static struct i2c_driver lcd_driver = {
    .driver = {
        .name  = DEVICE_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = lcd_probe,
    .remove   = lcd_remove,
    .id_table = lcd_id,
};

module_i2c_driver(lcd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YourName");
MODULE_DESCRIPTION("I2C LCD1602 Driver (lcd_i2c)"); 

