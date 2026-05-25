#include <stdint.h>
#include <stdbool.h>
#include "duart.h"
#include "mc68331.h"

void send_byte_to_host(uint8_t data)
{
    while ((SCSR & 0x0100) == 0)
        ;
    SCDR = data;
}

void control_led_and_send(uint8_t value)
{
    send_byte_to_host(0x1b);
    send_byte_to_host('0' + value);

    uint32_t count = 0;
    uint8_t toggle = 0;
    uint8_t step = 0;

    while (step < (value * 2))
    {
        count++;
        if (count > 0x3a98)
        {
            toggle ^= 1;
            count = 0;
            step++;
        }

        if (toggle)
        {
            if (SCSR & 0x0100)
                SCDR = 0xff;
            OPR_SET = 0x60;
        }
        else
        {
            OPR_CLR = 0x60;
        }
    }
}

void init_duart(uint32_t slow_mode)
{
    OPCR = 0x00;
    ACR = 0xf0;
    CRA = 0x3a;
    CRA = 0x2a;
    MR1A = 0x13;
    MR1A = 0x07;

    if (!slow_mode)
    {
        CSRA = 0xcc;
        CRA = 0x90;
        CRA = 0xb0;
    }
    else
    {
        CSRA = 0x88;
        CRA = 0x80;
        CRA = 0xa0;
    }
    CRA = 0x1a;
    CRA = 0x45;
}

void main_task(void)
{
    init_duart(0);

    control_led_and_send(2);

    while (true)
    {
        control_led_and_send(1);
        control_led_and_send(2);
    }
}

int main(void)
{
    SIMCR = 0x40c5;
    SYPCR = 0x4c;
    SYNCR = 0x7f05;
    CSORBT = 0x6c70;

    CSOR8 = 0x2ff0;
    CSOR9 = 0x7bf0;
    CSOR10 = 0x37f0;
    PORTE0 = 0x00;
    PORTE1 = 0x00;
    PORTF0 = 0x00;
    PORTF1 = 0x00;
    DDRE = 0xf8;
    DDRF = 0x00;
    PEPAR = 0xff;
    PFPAR = 0x60;
    PICR = 0xf;
    PITR = 0x00;
    QSMCR = 0x8a;
    QILR = 0x0550;
    PORTQS = 0x00;
    PQSPAR = 0x82;

    CR0 = 0x00;
    CR1 = 0x00;
    CR2 = 0x00;
    CR3 = 0x00;
    CR4 = 0x00;
    CR5 = 0x00;
    CR6 = 0x00;
    CR7 = 0x00;

    SPCR0 = 0x0104;
    SPCR1 = 0x0404;
    SPCR2 = 0x00;
    SPCR3 = 0x00;
    SCCR0 = 0x09;
    SCCR1 = 0x2c;

    GPTMCR = 0x80;
    ICR = 0x00;
    TOC1 = 0xFFFF;
    TOC2 = 0xFFFF;
    TOC3 = 0xFFFF;
    TOC4 = 0xFFFF;
    TI4_O5 = 0xFFFF;
    OC1 = 0x00;
    TCTL = 0x00;
    GP = 0x7000;

    main_task();

    return 0;
}
