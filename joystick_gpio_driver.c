#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/input-event-codes.h>


#define JOYSTICK_PCF8591_ADDR 0x48
#define JOYSTICK_X_REG 0
#define JOYSTICK_Y_REG 1

#define JOYSTICK_BUTTON_PIN 17

struct joystick_dev {
    struct i2c_client* client;
    struct input_dev* input;
    struct delayed_work poll_work;
    int irq;
};

static inline int joystick_read(struct i2c_client *client, u8 reg)
{
    s32 data = i2c_smbus_read_byte_data(client, reg);
    if (data < 0) return 0; // assume neutral position if error
    return data - 128;
}

static irqreturn_t joystick_button_irq(int irq, void* device)
{
    struct joystick_dev* joystick = device;

    int state = gpio_get_value(JOYSTICK_BUTTON_PIN);
    input_report_key(joystick->input, BTN_JOYSTICK, state);
    input_sync(joystick->input);
    return IRQ_HANDLED;
}

static void joystick_poll(struct work_struct* work)
{
    struct joystick_dev *joy_dev = container_of(work, struct joystick_dev, poll_work.work);
    struct i2c_client* client = joy_dev->client;

    int x = joystick_read(client, JOYSTICK_X_REG);
    int y = joystick_read(client, JOYSTICK_Y_REG);
    printk(KERN_DEBUG "Joystick poll coords, x: %d, y: %d\n", x, y);

    input_report_abs(joy_dev->input, ABS_X, x);
    input_report_abs(joy_dev->input, ABS_Y, y);
    input_sync(joy_dev->input);
    schedule_delayed_work(&joy_dev->poll_work, msecs_to_jiffies(50));
    return;
}

static int joystick_probe(struct i2c_client* client, const struct i2c_device_id *id)
{
    int error;
    struct joystick_dev *joystick_dev = devm_kzalloc(&client->dev, sizeof(struct joystick_dev), GFP_KERNEL);
    printk(KERN_INFO "Joystick probe started\n");
    if (!joystick_dev) return -ENOMEM;

    joystick_dev->client = client;
    
    joystick_dev->input = devm_input_allocate_device(&client->dev);
    if (!joystick_dev->input) return -ENOMEM;

    joystick_dev->input->name = "joystick_mouse";
    joystick_dev->input->id.bustype = BUS_I2C;

    input_set_capability(joystick_dev->input, EV_ABS, ABS_X);
    input_set_capability(joystick_dev->input, EV_ABS, ABS_Y);
    input_set_capability(joystick_dev->input, EV_KEY, BTN_JOYSTICK);

    input_set_abs_params(joystick_dev->input, ABS_X, -128, 127, 2, 4);
    input_set_abs_params(joystick_dev->input, ABS_Y, -128, 127, 2, 4);

    error = input_register_device(joystick_dev->input);
    if (error) {
        printk(KERN_ERR "Joystick failed to register input device. Code: %d\n", error);
        return error;
    }

    // FIX: currently the gpio_request_one function fails with error 517,
    // error = gpio_request_one(JOYSTICK_BUTTON_PIN, GPIOF_IN, "joystick button");
    // if (error) {
    //     printk(KERN_ERR "Joystick failed to request GPIO. Code: %d\n", error);
    //     return error;
    // }

    // joystick_dev->irq = gpio_to_irq(JOYSTICK_BUTTON_PIN);
    // // NOTE: should it be TRIGGER_RISING or TRIGGER_FALLING?
    // error = request_irq(joystick_dev->irq, joystick_button_irq, IRQF_TRIGGER_RISING, "joystick_irq", joystick_dev);
    // if (error) {
    //     printk(KERN_ERR "Joystick failed initialise irq\n");
    //     return error;
    // }

    INIT_DELAYED_WORK(&joystick_dev->poll_work, joystick_poll);
    schedule_delayed_work(&joystick_dev->poll_work, msecs_to_jiffies(50));

    i2c_set_clientdata(client, joystick_dev);
    printk(KERN_INFO "Joystick GPIO module probing succeeded!\n");
    return 0;
}

static void joystick_remove(struct i2c_client* client)
{
    struct joystick_dev* joy_dev = i2c_get_clientdata(client);
    free_irq(joy_dev->irq, joy_dev);
    gpio_free(JOYSTICK_BUTTON_PIN);
}

static const struct i2c_device_id joystick_id[] = {
    {"joystick_mouse", 0},
    {} // marks the end of an array
};
MODULE_DEVICE_TABLE(i2c, joystick_id);

static struct i2c_driver joystick_driver = {
    .driver = {
        .name = "joystick_mouse"
    },
    .probe = joystick_probe,
    .remove = joystick_remove,
    .id_table = joystick_id,
};

module_i2c_driver(joystick_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiachen Wang");
MODULE_DESCRIPTION("Joystick GPIO driver");




