/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2016-2023 Makerdiary
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

#define MAX_REC_COUNT        3
#define NDEF_MSG_BUF_SIZE    128

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Text message in English with its language code. */
static const uint8_t en_payload[] = {
    'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'
};
static const uint8_t en_code[] = {'e', 'n'};

/* Text message in Norwegian with its language code. */
static const uint8_t no_payload[] = {
    'H', 'a', 'l', 'l', 'o', ' ', 'V', 'e', 'r', 'd', 'e', 'n', '!'
};
static const uint8_t no_code[] = {'N', 'O'};

/* Text message in Polish with its language code. */
static const uint8_t pl_payload[] = {
    'W', 'i', 't', 'a', 'j', ' ', 0xc5, 0x9a, 'w', 'i', 'e', 'c', 'i',
    'e', '!'
};
static const uint8_t pl_code[] = {'P', 'L'};

/* Buffer used to hold an NFC NDEF message. */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];


static void nfc_callback(void *context,
             nfc_t2t_event_t event,
             const uint8_t *data,
             size_t data_length)
{
    ARG_UNUSED(context);
    ARG_UNUSED(data);
    ARG_UNUSED(data_length);

    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        gpio_pin_set_dt(&led, 1);
        break;
    case NFC_T2T_EVENT_FIELD_OFF:
        gpio_pin_set_dt(&led, 0);
        break;
    default:
        break;
    }
}


/**
 * @brief Function for encoding the NDEF text message.
 */
static int welcome_msg_encode(uint8_t *buffer, uint32_t *len)
{
    int err;

    /* Create NFC NDEF text record description in English */
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_en_text_rec,
                      UTF_8,
                      en_code,
                      sizeof(en_code),
                      en_payload,
                      sizeof(en_payload));

    /* Create NFC NDEF text record description in Norwegian */
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_no_text_rec,
                      UTF_8,
                      no_code,
                      sizeof(no_code),
                      no_payload,
                      sizeof(no_payload));

    /* Create NFC NDEF text record description in Polish */
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_pl_text_rec,
                      UTF_8,
                      pl_code,
                      sizeof(pl_code),
                      pl_payload,
                      sizeof(pl_payload));

    /* Create NFC NDEF message description, capacity - MAX_REC_COUNT
     * records
     */
    NFC_NDEF_MSG_DEF(nfc_text_msg, MAX_REC_COUNT);

    /* Add text records to NDEF text message */
    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
                   &NFC_NDEF_TEXT_RECORD_DESC(nfc_en_text_rec));
    if (err < 0) {
        printk("Cannot add first record!\n");
        return err;
    }
    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
                   &NFC_NDEF_TEXT_RECORD_DESC(nfc_no_text_rec));
    if (err < 0) {
        printk("Cannot add second record!\n");
        return err;
    }
    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
                   &NFC_NDEF_TEXT_RECORD_DESC(nfc_pl_text_rec));
    if (err < 0) {
        printk("Cannot add third record!\n");
        return err;
    }

    err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg),
                      buffer,
                      len);
    if (err < 0) {
        printk("Cannot encode message!\n");
    }

    return err;
}


int main(void)
{
    printk("Starting NFC Text Record sample\n");

    /* Configure LED */
    if (!device_is_ready(led.port)) {
        return -ENOTSUP;
    }
    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        return err;
    }

    /* Set up NFC */
    if (nfc_t2t_setup(nfc_callback, NULL) < 0) {
        printk("Cannot setup NFC T2T library!\n");
        goto fail;
    }

    uint32_t len = sizeof(ndef_msg_buf);

    /* Encode welcome message */
    if (welcome_msg_encode(ndef_msg_buf, &len) < 0) {
        printk("Cannot encode message!\n");
        goto fail;
    }


    /* Set created message as the NFC payload */
    if (nfc_t2t_payload_set(ndef_msg_buf, len) < 0) {
        printk("Cannot set payload!\n");
        goto fail;
    }


    /* Start sensing NFC field */
    if (nfc_t2t_emulation_start() < 0) {
        printk("Cannot start emulation!\n");
        goto fail;
    }
    printk("NFC configuration done\n");

    return 0;

fail:
#if CONFIG_REBOOT
    sys_reboot(SYS_REBOOT_COLD);
#endif /* CONFIG_REBOOT */

    return -EIO;
}
