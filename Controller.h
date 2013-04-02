/* ============================================================================
 *  Controller.h: Audio controller.
 *
 *  AUDIOSIM: AUDIO Interface SIMulator.
 *  Copyright (C) 2013, Tyler J. Stachecki.
 *  All rights reserved.
 *
 *  This file is subject to the terms and conditions defined in
 *  file 'LICENSE', which is part of this source code package.
 * ========================================================================= */
#ifndef __AIF__CONTROLLER_H__
#define __AIF__CONTROLLER_H__
#include "Address.h"
#include "Common.h"

enum AIRegister {
#define X(reg) reg,
#include "Registers.md"
#undef X
  NUM_AI_REGISTERS
};

#ifndef NDEBUG
extern const char *AIRegisterMnemonics[NUM_AI_REGISTERS];
#endif

struct BusController;

struct AIFController {
  struct BusController *bus;

  const uint8_t *rom;
  uint32_t regs[NUM_AI_REGISTERS];
};

struct AIFController *CreateAIF(void);
void DestroyAIF(struct AIFController *);

#endif

