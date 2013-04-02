/* ============================================================================
 *  Controller.c: Audio controller.
 *
 *  AUDIOSIM: AUDIO Interface SIMulator.
 *  Copyright (C) 2013, Tyler J. Stachecki.
 *  All rights reserved.
 *
 *  This file is subject to the terms and conditions defined in
 *  file 'LICENSE', which is part of this source code package.
 * ========================================================================= */
#include "Address.h"
#include "Common.h"
#include "Controller.h"
#include "Definitions.h"
#include "Externs.h"

#ifdef __cplusplus
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#define DACRATE_NTSC 48681812
static void InitAIF(struct AIFController *);

/* ============================================================================
 *  Mnemonics table.
 * ========================================================================= */
#ifndef NDEBUG
const char *AIRegisterMnemonics[NUM_AI_REGISTERS] = {
#define X(reg) #reg,
#include "Registers.md"
#undef X
};
#endif

/* ============================================================================
 *  ConnectAIFToBus: Connects a AIF instance to a Bus instance.
 * ========================================================================= */
void
ConnectAIFToBus(struct AIFController *controller, struct BusController *bus) {
  controller->bus = bus;
}

/* ============================================================================
 *  CreateAIF: Creates and initializes an AIF instance.
 * ========================================================================= */
struct AIFController *
CreateAIF(void) {
  struct AIFController *controller;

  /* Allocate memory for controller. */
  if ((controller = (struct AIFController*) malloc(
    sizeof(struct AIFController))) == NULL) {
    return NULL;
  }

  InitAIF(controller);
  return controller;
}

/* ============================================================================
 *  DestroyAIF: Releases any resources allocated for an AIF instance.
 * ========================================================================= */
void
DestroyAIF(struct AIFController *controller) {
  free(controller);
}

/* ============================================================================
 *  InitAIF: Initializes the AIF controller.
 * ========================================================================= */
static void
InitAIF(struct AIFController *controller) {
  debug("Initializing AIF.");
  memset(controller, 0, sizeof(*controller));
}

/* ============================================================================
 *  AIRegRead: Read from AI registers.
 * ========================================================================= */
int
AIRegRead(void *_controller, uint32_t address, void *_data) {
	struct AIFController *controller = (struct AIFController*) _controller;
	uint32_t *data = (uint32_t*) _data;

  address -= AI_REGS_BASE_ADDRESS;
  enum AIRegister reg = (enum AIRegister) (address / 4);

  debugarg("AIRegRead: Reading from register [%s].", AIRegisterMnemonics[reg]);

  switch(reg) {
  case AI_LEN_REG:
    if (controller->regs[AI_STATUS_REG] & 0x80000001)
      *data = controller->regs[AI_LEN_REG];

    else if (controller->regs[AI_STATUS_REG] & 0x40000000) {
      uint32_t rate = controller->regs[AI_DACRATE_REG] + 1;
      unsigned samples = (double) DACRATE_NTSC / rate;
      *data = samples * 5 /* seconds */ * 4;
    }

    else
      *data = 0;

    break;

  default:
    *data = controller->regs[reg];
    break;
  }

  return 0;
}

/* ============================================================================
 *  AIRegWrite: Write to AI registers.
 * ========================================================================= */
int
AIRegWrite(void *_controller, uint32_t address, void *_data) {
	struct AIFController *controller = (struct AIFController*) _controller;
	uint32_t *data = (uint32_t*) _data;

  address -= AI_REGS_BASE_ADDRESS;
  enum AIRegister reg = (enum AIRegister) (address / 4);

  debugarg("AIRegWrite: Writing to register [%s].", AIRegisterMnemonics[reg]);

  switch(reg) {
  case AI_DRAM_ADDR_REG:
    controller->regs[AI_DRAM_ADDR_REG] = *data & 0xFFFFF8;
    break;

  case AI_LEN_REG:
    controller->regs[AI_LEN_REG] = *data & 0x3FFFF;
    controller->regs[AI_STATUS_REG] |= 0x80000001;

    if (!(controller->regs[AI_STATUS_REG] & 0x40000000))
      BusRaiseRCPInterrupt(controller->bus, MI_INTR_AI);

    break;

  case AI_STATUS_REG:
    BusClearRCPInterrupt(controller->bus, MI_INTR_AI);
    break;

  case AI_DACRATE_REG:
    controller->regs[AI_DACRATE_REG] = *data & 0x3FFF;
    break;

  case AI_BITRATE_REG:
    controller->regs[AI_BITRATE_REG] = *data & 0xF;
    break;

  default:
    controller->regs[reg] = *data;
    break;
  }

  return 0;
}

