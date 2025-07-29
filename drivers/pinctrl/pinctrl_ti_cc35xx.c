/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc35xx_pinctrl

#include <zephyr/arch/cpu.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/pinctrl/ti-cc35xx-pinctrl.h>

#define MEM_GPIO_PAD_CONFIG_MSK 0xFFFU
#define MEM_GPIO0_PCFG_OFFSET   0x0002D004

#define MEM_GPIO0_CFG_OFFSET  0
#define MEM_GPIO0_PCTL_OFFSET 4
#define MEM_GPIO_NEXT_OFFSET  0x1000
#define MEM_GPIO_IE_BIT       11
#define MEM_GPIO_OUTDIS_BIT   12

static void pinctrl_configure_pin(pinctrl_soc_pin_t pincfg)
{
	uint8_t pin;
	mem_addr_t reg;
	uint32_t tmp;
	int dir;

	pin = (pincfg >> TI_CC35XX_PIN_POS) & TI_CC35XX_PIN_MSK;
	dir = TI_CC35XX_PINCTRL_DIR(pincfg);

	/* set pin function */
	reg = DT_INST_REG_ADDR(0) + MEM_GPIO0_PCFG_OFFSET + pin * sizeof(uint32_t);
	sys_write32(pincfg & TI_CC35XX_MUX_MSK, reg);

	/* enable/disable receiver operation on pin */
	reg = DT_INST_REG_ADDR(0) + pin * MEM_GPIO_NEXT_OFFSET + MEM_GPIO0_CFG_OFFSET;
	tmp = sys_read32(reg);
	if (dir == TI_CC35XX_PINCTRL_DIR_IN) {
		tmp |= BIT(MEM_GPIO_IE_BIT) | BIT(MEM_GPIO_OUTDIS_BIT);
	} else {
		tmp &= ~(BIT(MEM_GPIO_IE_BIT) | BIT(MEM_GPIO_OUTDIS_BIT));
	}
	sys_write32(tmp, reg);

	/* give IP control over pull */
	reg = DT_INST_REG_ADDR(0) + pin * MEM_GPIO_NEXT_OFFSET + MEM_GPIO0_PCTL_OFFSET;
	sys_write32(0, reg);
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(reg);

	for (uint8_t i = 0; i < pin_cnt; i++) {
		pinctrl_configure_pin(pins[i]);
	}

	return 0;
}
