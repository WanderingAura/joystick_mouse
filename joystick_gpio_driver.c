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

static struct input_dev* joystick_dev;
struct joystick_dev {
    struct i2c_client* client;
    struct input_dev* input;
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

static int joystick_probe(struct i2c_client* client, const struct i2c_device_id *id)
{
    int error;
    struct joystick_dev *joystick_dev = devm_kzalloc(&client->dev, sizeof(struct joystick_dev), GFP_KERNEL);
    if (!joystick_dev) return -ENOMEM;

    joystick_dev->client = client;
    
    joystick_dev->input = devm_input_allocate_device(&client->dev);
    if (!joystick_dev->input) return -ENOMEM;

    joystick_dev->input->name = "Joystick Mouse";
    joystick_dev->input->id.bustype = BUS_I2C;

    input_set_capability(joystick_dev->input, EV_ABS, ABS_X);
    input_set_capability(joystick_dev->input, EV_ABS, ABS_Y);
    input_set_capability(joystick_dev->input, EV_KEY, BTN_JOYSTICK);

    input_set_abs_params(joystick_dev->input, ABS_X, -128, 127, 2, 4);
    input_set_abs_params(joystick_dev->input, ABS_Y, -128, 127, 2, 4);

    error = input_register_device(joystick_dev->input);
    if (error) return error;

    error = gpio_request_one(JOYSTICK_BUTTON_PIN, GPIOF_IN, "joystick button");
    if (error) return error;

    joystick_dev->irq = gpio_to_irq(JOYSTICK_BUTTON_PIN);
    // NOTE: should it be TRIGGER_RISING or TRIGGER_FALLING?
    error = request_irq(joystick_dev->irq, joystick_button_irq, IRQF_TRIGGER_RISING, "joystick_irq", joystick_dev);
    if (error) return error;

    i2c_set_clientdata(client, joystick_dev);
    return 0;
}

static void joystick_remove(struct i2c_client* client)
{
    struct joystick_dev* joystick_dev = i2c_get_clientdata(client);
    free_irq(joystick_dev->irq, joystick_dev);
    gpio_free(JOYSTICK_BUTTON_PIN);
}

// TODO: write polling function
static void joystick_poll(struct work_struct* work)
{
    return;
}

static const struct i2c_device_id joystick_id[] = {
    {"joystick_id", 0},
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




