/*
 * ASPEED SoC 2600 family
 *
 * Copyright (c) 2016-2019, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/misc/unimp.h"
#include "hw/arm/aspeed_soc.h"
#include "hw/char/serial.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "hw/i2c/aspeed_i2c.h"
#include "net/net.h"
#include "sysemu/sysemu.h"

#define ASPEED_SOC_IOMEM_SIZE       0x00200000

static const hwaddr aspeed_soc_ast2600_memmap[] = {
    [ASPEED_DEV_SRAM]      = 0x10000000,
    /* 0x16000000     0x17FFFFFF : AHB BUS do LPC Bus bridge */
    [ASPEED_DEV_IOMEM]     = 0x1E600000,
    [ASPEED_DEV_PWM]       = 0x1E610000,
    [ASPEED_DEV_FMC]       = 0x1E620000,
    [ASPEED_DEV_SPI1]      = 0x1E630000,
    [ASPEED_DEV_SPI2]      = 0x1E641000,
    [ASPEED_DEV_EHCI1]     = 0x1E6A1000,
    [ASPEED_DEV_EHCI2]     = 0x1E6A3000,
    [ASPEED_DEV_MII1]      = 0x1E650000,
    [ASPEED_DEV_MII2]      = 0x1E650008,
    [ASPEED_DEV_MII3]      = 0x1E650010,
    [ASPEED_DEV_MII4]      = 0x1E650018,
    [ASPEED_DEV_ETH1]      = 0x1E660000,
    [ASPEED_DEV_ETH3]      = 0x1E670000,
    [ASPEED_DEV_ETH2]      = 0x1E680000,
    [ASPEED_DEV_ETH4]      = 0x1E690000,
    [ASPEED_DEV_VIC]       = 0x1E6C0000,
    [ASPEED_DEV_HACE]      = 0x1E6D0000,
    [ASPEED_DEV_SDMC]      = 0x1E6E0000,
    [ASPEED_DEV_SCU]       = 0x1E6E2000,
    [ASPEED_DEV_XDMA]      = 0x1E6E7000,
    [ASPEED_DEV_ADC]       = 0x1E6E9000,
    [ASPEED_DEV_VIDEO]     = 0x1E700000,
    [ASPEED_DEV_SDHCI]     = 0x1E740000,
    [ASPEED_DEV_EMMC]      = 0x1E750000,
    [ASPEED_DEV_GPIO]      = 0x1E780000,
    [ASPEED_DEV_GPIO_1_8V] = 0x1E780800,
    [ASPEED_DEV_RTC]       = 0x1E781000,
    [ASPEED_DEV_TIMER1]    = 0x1E782000,
    [ASPEED_DEV_WDT]       = 0x1E785000,
    [ASPEED_DEV_LPC]       = 0x1E789000,
    [ASPEED_DEV_IBT]       = 0x1E789140,
    [ASPEED_DEV_I2C]       = 0x1E78A000,
    [ASPEED_DEV_UART1]     = 0x1E783000,
    [ASPEED_DEV_UART5]     = 0x1E784000,
    [ASPEED_DEV_VUART]     = 0x1E787000,
    [ASPEED_DEV_FSI1]      = 0x1E79B000,
    [ASPEED_DEV_FSI2]      = 0x1E79B100,
    [ASPEED_DEV_SDRAM]     = 0x80000000,
};

#define ASPEED_A7MPCORE_ADDR 0x40460000

#define AST2600_MAX_IRQ 197

/* Shared Peripheral Interrupt values below are offset by -32 from datasheet */
static const int aspeed_soc_ast2600_irqmap[] = {
    [ASPEED_DEV_UART1]     = 47,
    [ASPEED_DEV_UART2]     = 48,
    [ASPEED_DEV_UART3]     = 49,
    [ASPEED_DEV_UART4]     = 50,
    [ASPEED_DEV_UART5]     = 8,
    [ASPEED_DEV_VUART]     = 8,
    [ASPEED_DEV_FMC]       = 39,
    [ASPEED_DEV_SDMC]      = 0,
    [ASPEED_DEV_SCU]       = 12,
    [ASPEED_DEV_ADC]       = 78,
    [ASPEED_DEV_XDMA]      = 6,
    [ASPEED_DEV_SDHCI]     = 43,
    [ASPEED_DEV_EHCI1]     = 5,
    [ASPEED_DEV_EHCI2]     = 9,
    [ASPEED_DEV_EMMC]      = 15,
    [ASPEED_DEV_GPIO]      = 40,
    [ASPEED_DEV_GPIO_1_8V] = 11,
    [ASPEED_DEV_RTC]       = 13,
    [ASPEED_DEV_TIMER1]    = 16,
    [ASPEED_DEV_TIMER2]    = 17,
    [ASPEED_DEV_TIMER3]    = 18,
    [ASPEED_DEV_TIMER4]    = 19,
    [ASPEED_DEV_TIMER5]    = 20,
    [ASPEED_DEV_TIMER6]    = 21,
    [ASPEED_DEV_TIMER7]    = 22,
    [ASPEED_DEV_TIMER8]    = 23,
    [ASPEED_DEV_WDT]       = 24,
    [ASPEED_DEV_PWM]       = 44,
    [ASPEED_DEV_LPC]       = 35,
    [ASPEED_DEV_IBT]       = 143,
    [ASPEED_DEV_I2C]       = 110,   /* 110 -> 125 */
    [ASPEED_DEV_ETH1]      = 2,
    [ASPEED_DEV_ETH2]      = 3,
    [ASPEED_DEV_HACE]      = 4,
    [ASPEED_DEV_ETH3]      = 32,
    [ASPEED_DEV_ETH4]      = 33,
    [ASPEED_DEV_KCS]       = 138,   /* 138 -> 142 */
    [ASPEED_DEV_FSI1]      = 100,
    [ASPEED_DEV_FSI2]      = 101,
};

static qemu_irq aspeed_soc_get_irq(AspeedSoCState *s, int ctrl)
{
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);

    return qdev_get_gpio_in(DEVICE(&s->a7mpcore), sc->irqmap[ctrl]);
}

static void aspeed_soc_ast2600_init(Object *obj)
{
    AspeedSoCState *s = ASPEED_SOC(obj);
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);
    int i;
    char socname[8];
    char typename[64];

    if (sscanf(sc->name, "%7s", socname) != 1) {
        g_assert_not_reached();
    }

    for (i = 0; i < sc->num_cpus; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpu[i], sc->cpu_type);
    }

    snprintf(typename, sizeof(typename), "aspeed.scu-%s", socname);
    object_initialize_child(obj, "scu", &s->scu, typename);
    qdev_prop_set_uint32(DEVICE(&s->scu), "silicon-rev",
                         sc->silicon_rev);
    object_property_add_alias(obj, "hw-strap1", OBJECT(&s->scu),
                              "hw-strap1");
    object_property_add_alias(obj, "hw-strap2", OBJECT(&s->scu),
                              "hw-strap2");
    object_property_add_alias(obj, "hw-prot-key", OBJECT(&s->scu),
                              "hw-prot-key");

    object_initialize_child(obj, "a7mpcore", &s->a7mpcore,
                            TYPE_A15MPCORE_PRIV);

    object_initialize_child(obj, "rtc", &s->rtc, TYPE_ASPEED_RTC);

    snprintf(typename, sizeof(typename), "aspeed.timer-%s", socname);
    object_initialize_child(obj, "timerctrl", &s->timerctrl, typename);

    snprintf(typename, sizeof(typename), "aspeed.adc-%s", socname);
    object_initialize_child(obj, "adc", &s->adc, typename);

    snprintf(typename, sizeof(typename), "aspeed.i2c-%s", socname);
    object_initialize_child(obj, "i2c", &s->i2c, typename);

    snprintf(typename, sizeof(typename), "aspeed.fmc-%s", socname);
    object_initialize_child(obj, "fmc", &s->fmc, typename);
    object_property_add_alias(obj, "num-cs", OBJECT(&s->fmc), "num-cs");

    for (i = 0; i < sc->spis_num; i++) {
        snprintf(typename, sizeof(typename), "aspeed.spi%d-%s", i + 1, socname);
        object_initialize_child(obj, "spi[*]", &s->spi[i], typename);
    }

    for (i = 0; i < sc->ehcis_num; i++) {
        object_initialize_child(obj, "ehci[*]", &s->ehci[i],
                                TYPE_PLATFORM_EHCI);
    }

    snprintf(typename, sizeof(typename), "aspeed.sdmc-%s", socname);
    object_initialize_child(obj, "sdmc", &s->sdmc, typename);
    object_property_add_alias(obj, "ram-size", OBJECT(&s->sdmc),
                              "ram-size");
    object_property_add_alias(obj, "max-ram-size", OBJECT(&s->sdmc),
                              "max-ram-size");

    for (i = 0; i < sc->wdts_num; i++) {
        snprintf(typename, sizeof(typename), "aspeed.wdt-%s", socname);
        object_initialize_child(obj, "wdt[*]", &s->wdt[i], typename);
    }

    for (i = 0; i < sc->macs_num; i++) {
        object_initialize_child(obj, "ftgmac100[*]", &s->ftgmac100[i],
                                TYPE_FTGMAC100);

        object_initialize_child(obj, "mii[*]", &s->mii[i], TYPE_ASPEED_MII);
    }

    snprintf(typename, sizeof(typename), TYPE_ASPEED_XDMA "-%s", socname);
    object_initialize_child(obj, "xdma", &s->xdma, typename);

    snprintf(typename, sizeof(typename), "aspeed.gpio-%s", socname);
    object_initialize_child(obj, "gpio", &s->gpio, typename);

    snprintf(typename, sizeof(typename), "aspeed.gpio-%s-1_8v", socname);
    object_initialize_child(obj, "gpio_1_8v", &s->gpio_1_8v, typename);

    object_initialize_child(obj, "sd-controller", &s->sdhci,
                            TYPE_ASPEED_SDHCI);

    object_property_set_int(OBJECT(&s->sdhci), "num-slots", 2, &error_abort);

    /* Init sd card slot class here so that they're under the correct parent */
    for (i = 0; i < ASPEED_SDHCI_NUM_SLOTS; ++i) {
        object_initialize_child(obj, "sd-controller.sdhci[*]",
                                &s->sdhci.slots[i], TYPE_SYSBUS_SDHCI);
    }

    object_initialize_child(obj, "emmc-controller", &s->emmc,
                            TYPE_ASPEED_SDHCI);

    object_property_set_int(OBJECT(&s->emmc), "num-slots", 1, &error_abort);

    object_initialize_child(obj, "emmc-controller.sdhci", &s->emmc.slots[0],
                            TYPE_SYSBUS_SDHCI);

    object_initialize_child(obj, "lpc", &s->lpc, TYPE_ASPEED_LPC);

    snprintf(typename, sizeof(typename), "aspeed.hace-%s", socname);
    object_initialize_child(obj, "hace", &s->hace, typename);

    object_initialize_child(obj, "pwm", &s->pwm, TYPE_ASPEED_PWM);

    object_initialize_child(obj, "fsi[*]", &s->fsi[0], TYPE_ASPEED_APB2OPB);
}

/*
 * ASPEED ast2600 has 0xf as cluster ID
 *
 * https://developer.arm.com/documentation/ddi0388/e/the-system-control-coprocessors/summary-of-system-control-coprocessor-registers/multiprocessor-affinity-register
 */
static uint64_t aspeed_calc_affinity(int cpu)
{
    return (0xf << ARM_AFF1_SHIFT) | cpu;
}

static void aspeed_soc_ast2600_realize(DeviceState *dev, Error **errp)
{
    int i;
    AspeedSoCState *s = ASPEED_SOC(dev);
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);
    Error *err = NULL;
    qemu_irq irq;

    /* IO space */
    create_unimplemented_device("aspeed_soc.io", sc->memmap[ASPEED_DEV_IOMEM],
                                ASPEED_SOC_IOMEM_SIZE);

    /* Video engine stub */
    create_unimplemented_device("aspeed.video", sc->memmap[ASPEED_DEV_VIDEO],
                                0x1000);

    /* CPU */
    for (i = 0; i < sc->num_cpus; i++) {
        if (sc->num_cpus > 1) {
            object_property_set_int(OBJECT(&s->cpu[i]), "reset-cbar",
                                    ASPEED_A7MPCORE_ADDR, &error_abort);
        }
        object_property_set_int(OBJECT(&s->cpu[i]), "mp-affinity",
                                aspeed_calc_affinity(i), &error_abort);

        object_property_set_int(OBJECT(&s->cpu[i]), "cntfrq", 1125000000,
                                &error_abort);

        if (!qdev_realize(DEVICE(&s->cpu[i]), NULL, errp)) {
            return;
        }
    }

    /* A7MPCORE */
    object_property_set_int(OBJECT(&s->a7mpcore), "num-cpu", sc->num_cpus,
                            &error_abort);
    object_property_set_int(OBJECT(&s->a7mpcore), "num-irq",
                            ROUND_UP(AST2600_MAX_IRQ + GIC_INTERNAL, 32),
                            &error_abort);

    sysbus_realize(SYS_BUS_DEVICE(&s->a7mpcore), &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->a7mpcore), 0, ASPEED_A7MPCORE_ADDR);

    for (i = 0; i < sc->num_cpus; i++) {
        SysBusDevice *sbd = SYS_BUS_DEVICE(&s->a7mpcore);
        DeviceState  *d   = DEVICE(qemu_get_cpu(i));

        irq = qdev_get_gpio_in(d, ARM_CPU_IRQ);
        sysbus_connect_irq(sbd, i, irq);
        irq = qdev_get_gpio_in(d, ARM_CPU_FIQ);
        sysbus_connect_irq(sbd, i + sc->num_cpus, irq);
        irq = qdev_get_gpio_in(d, ARM_CPU_VIRQ);
        sysbus_connect_irq(sbd, i + 2 * sc->num_cpus, irq);
        irq = qdev_get_gpio_in(d, ARM_CPU_VFIQ);
        sysbus_connect_irq(sbd, i + 3 * sc->num_cpus, irq);
    }

    /* SRAM */
    memory_region_init_ram(&s->sram, OBJECT(dev), "aspeed.sram",
                           sc->sram_size, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(),
                                sc->memmap[ASPEED_DEV_SRAM], &s->sram);

    /* SCU */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->scu), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->scu), 0, sc->memmap[ASPEED_DEV_SCU]);

    /* RTC */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->rtc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->rtc), 0, sc->memmap[ASPEED_DEV_RTC]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->rtc), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_RTC));

    /* Timer */
    object_property_set_link(OBJECT(&s->timerctrl), "scu", OBJECT(&s->scu),
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->timerctrl), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->timerctrl), 0,
                    sc->memmap[ASPEED_DEV_TIMER1]);
    for (i = 0; i < ASPEED_TIMER_NR_TIMERS; i++) {
        qemu_irq irq = aspeed_soc_get_irq(s, ASPEED_DEV_TIMER1 + i);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->timerctrl), i, irq);
    }

    /* ADC */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->adc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->adc), 0, sc->memmap[ASPEED_DEV_ADC]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->adc), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_ADC));

    /* UART - attach an 8250 to the IO space as our UART5 */
    serial_mm_init(get_system_memory(), sc->memmap[ASPEED_DEV_UART5], 2,
                   aspeed_soc_get_irq(s, ASPEED_DEV_UART5),
                   38400, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    /* I2C */
    object_property_set_link(OBJECT(&s->i2c), "dram", OBJECT(s->dram_mr),
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->i2c), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c), 0, sc->memmap[ASPEED_DEV_I2C]);
    for (i = 0; i < ASPEED_I2C_GET_CLASS(&s->i2c)->num_busses; i++) {
        qemu_irq irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                        sc->irqmap[ASPEED_DEV_I2C] + i);
        /* The AST2600 I2C controller has one IRQ per bus. */
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c.busses[i]), 0, irq);
    }

    /* FMC, The number of CS is set at the board level */
    object_property_set_link(OBJECT(&s->fmc), "wdt2", OBJECT(&s->wdt[2].iomem),
                             &error_abort);
    object_property_set_link(OBJECT(&s->fmc), "dram", OBJECT(s->dram_mr),
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->fmc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->fmc), 0, sc->memmap[ASPEED_DEV_FMC]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->fmc), 1,
                    ASPEED_SMC_GET_CLASS(&s->fmc)->flash_window_base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->fmc), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_FMC));

    /* SPI */
    for (i = 0; i < sc->spis_num; i++) {
        object_property_set_link(OBJECT(&s->spi[i]), "dram",
                                 OBJECT(s->dram_mr), &error_abort);
        object_property_set_int(OBJECT(&s->spi[i]), "num-cs", 1, &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 0,
                        sc->memmap[ASPEED_DEV_SPI1 + i]);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 1,
                        ASPEED_SMC_GET_CLASS(&s->spi[i])->flash_window_base);
    }

    /* EHCI */
    for (i = 0; i < sc->ehcis_num; i++) {
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->ehci[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->ehci[i]), 0,
                        sc->memmap[ASPEED_DEV_EHCI1 + i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->ehci[i]), 0,
                           aspeed_soc_get_irq(s, ASPEED_DEV_EHCI1 + i));
    }

    /* SDMC - SDRAM Memory Controller */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->sdmc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sdmc), 0, sc->memmap[ASPEED_DEV_SDMC]);

    /* Watch dog */
    for (i = 0; i < sc->wdts_num; i++) {
        AspeedWDTClass *awc = ASPEED_WDT_GET_CLASS(&s->wdt[i]);

        object_property_set_link(OBJECT(&s->wdt[i]), "scu", OBJECT(&s->scu),
                                 &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->wdt[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->wdt[i]), 0,
                        sc->memmap[ASPEED_DEV_WDT] + i * awc->offset);
    }

    /* Net */
    for (i = 0; i < sc->macs_num; i++) {
        object_property_set_bool(OBJECT(&s->ftgmac100[i]), "aspeed", true,
                                 &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->ftgmac100[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->ftgmac100[i]), 0,
                        sc->memmap[ASPEED_DEV_ETH1 + i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->ftgmac100[i]), 0,
                           aspeed_soc_get_irq(s, ASPEED_DEV_ETH1 + i));

        object_property_set_link(OBJECT(&s->mii[i]), "nic",
                                 OBJECT(&s->ftgmac100[i]), &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->mii[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->mii[i]), 0,
                        sc->memmap[ASPEED_DEV_MII1 + i]);
    }

    /* XDMA */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->xdma), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->xdma), 0,
                    sc->memmap[ASPEED_DEV_XDMA]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->xdma), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_XDMA));

    /* GPIO */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio), 0, sc->memmap[ASPEED_DEV_GPIO]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_GPIO));

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio_1_8v), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio_1_8v), 0,
                    sc->memmap[ASPEED_DEV_GPIO_1_8V]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio_1_8v), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_GPIO_1_8V));

    /* SDHCI */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->sdhci), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sdhci), 0,
                    sc->memmap[ASPEED_DEV_SDHCI]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->sdhci), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_SDHCI));

    /* eMMC */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->emmc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->emmc), 0, sc->memmap[ASPEED_DEV_EMMC]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->emmc), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_EMMC));

    /* LPC */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->lpc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->lpc), 0, sc->memmap[ASPEED_DEV_LPC]);

    /* Connect the LPC IRQ to the GIC. It is otherwise unused. */
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->lpc), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_LPC));

    /*
     * On the AST2600 LPC subdevice IRQs are connected straight to the GIC.
     *
     * LPC subdevice IRQ sources are offset from 1 because the LPC model caters
     * to the AST2400 and AST2500. SoCs before the AST2600 have one LPC IRQ
     * shared across the subdevices, and the shared IRQ output to the VIC is at
     * offset 0.
     */
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->lpc), 1 + aspeed_lpc_kcs_1,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                sc->irqmap[ASPEED_DEV_KCS] + aspeed_lpc_kcs_1));

    sysbus_connect_irq(SYS_BUS_DEVICE(&s->lpc), 1 + aspeed_lpc_kcs_2,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                sc->irqmap[ASPEED_DEV_KCS] + aspeed_lpc_kcs_2));

    sysbus_connect_irq(SYS_BUS_DEVICE(&s->lpc), 1 + aspeed_lpc_kcs_3,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                sc->irqmap[ASPEED_DEV_KCS] + aspeed_lpc_kcs_3));

    sysbus_connect_irq(SYS_BUS_DEVICE(&s->lpc), 1 + aspeed_lpc_kcs_4,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                sc->irqmap[ASPEED_DEV_KCS] + aspeed_lpc_kcs_4));

    /* HACE */
    object_property_set_link(OBJECT(&s->hace), "dram", OBJECT(s->dram_mr),
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->hace), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->hace), 0, sc->memmap[ASPEED_DEV_HACE]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->hace), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_HACE));

    /* PWM */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->pwm), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->pwm), 0, sc->memmap[ASPEED_DEV_PWM]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pwm), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_PWM));

    /* FSI */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->fsi[0]), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->fsi[0]), 0,
                    sc->memmap[ASPEED_DEV_FSI1]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->fsi[0]), 0,
                       aspeed_soc_get_irq(s, ASPEED_DEV_FSI1));
}

static void aspeed_soc_ast2600_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    AspeedSoCClass *sc = ASPEED_SOC_CLASS(oc);

    dc->realize      = aspeed_soc_ast2600_realize;

    sc->name         = "ast2600-a1";
    sc->cpu_type     = ARM_CPU_TYPE_NAME("cortex-a7");
    sc->silicon_rev  = AST2600_A1_SILICON_REV;
    sc->sram_size    = 0x16400;
    sc->spis_num     = 2;
    sc->ehcis_num    = 2;
    sc->wdts_num     = 4;
    sc->macs_num     = 4;
    sc->irqmap       = aspeed_soc_ast2600_irqmap;
    sc->memmap       = aspeed_soc_ast2600_memmap;
    sc->num_cpus     = 2;
}

static const TypeInfo aspeed_soc_ast2600_type_info = {
    .name           = "ast2600-a1",
    .parent         = TYPE_ASPEED_SOC,
    .instance_size  = sizeof(AspeedSoCState),
    .instance_init  = aspeed_soc_ast2600_init,
    .class_init     = aspeed_soc_ast2600_class_init,
    .class_size     = sizeof(AspeedSoCClass),
};

static void aspeed_soc_register_types(void)
{
    type_register_static(&aspeed_soc_ast2600_type_info);
};

type_init(aspeed_soc_register_types)
