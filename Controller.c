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

#include <AL/al.h>
#include <AL/alc.h>

static void FIFOPush(struct AIFController *);
static void FIFOPop(struct AIFController *);
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
 *  CycleAIF: Lets the AI know we are cycling the machine.
 * ========================================================================= */
void
CycleAIF(struct AIFController *controller) {}

/* ============================================================================
 *  DestroyAIF: Releases any resources allocated for an AIF instance.
 * ========================================================================= */
void
DestroyAIF(struct AIFController *controller) {
  if (controller->device) {
    alDeleteSources(1, &controller->source);
    alDeleteBuffers(AUDIO_DMA_DEPTH, controller->buffers);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(controller->context);
    alcCloseDevice(controller->device);
  }

  free(controller);
}

/* ============================================================================
 *  FIFOPush: Pushes an entry onto the AI FIFO.
 * ========================================================================= */
static void
FIFOPush(struct AIFController *controller) {
  uint32_t address = controller->regs[AI_DRAM_ADDR_REG];
  uint32_t length = controller->regs[AI_LEN_REG];
  struct AudioFIFOEntry *fifoEntry;

  assert(controller->fifoWritePosition < AUDIO_DMA_DEPTH);
  fifoEntry = &controller->fifo[controller->fifoWritePosition];
  fifoEntry->address = address;
  fifoEntry->length = length;

  controller->fifoWritePosition++;
  controller->fifoEntryCount++;

  if (controller->fifoWritePosition == AUDIO_DMA_DEPTH)
    controller->fifoWritePosition = 0;

  if (controller->fifoEntryCount == AUDIO_DMA_DEPTH)
    controller->regs[AI_STATUS_REG] |= 0x80000001;

  if (!(controller->regs[AI_STATUS_REG] & 0x40000000)) {
    BusRaiseRCPInterrupt(controller->bus, MI_INTR_AI);
    controller->regs[AI_STATUS_REG] |= 0x40000000;
  }
}

/* ============================================================================
 *  FIFOPop: Pops an entry off the AI FIFO.
 * ========================================================================= */
static void
FIFOPop(struct AIFController *controller) {
  controller->fifoEntryCount--;
  controller->fifoReadPosition++;

  if (controller->fifoReadPosition >= AUDIO_DMA_DEPTH)
    controller->fifoReadPosition = 0;

  else if (controller->fifoReadPosition < AUDIO_DMA_DEPTH) {
    BusRaiseRCPInterrupt(controller->bus, MI_INTR_AI);
    controller->regs[AI_STATUS_REG] &= ~0x80000001;
  }
}

/* ============================================================================
 *  InitAIF: Initializes the AIF controller.
 * ========================================================================= */
static void
InitAIF(struct AIFController *controller) {
  debug("Initializing AIF.");
  memset(controller, 0, sizeof(*controller));

  /* Try to initialize OpenAL; return if we can't. */
  if ((controller->device = alcOpenDevice(NULL)) == NULL) {
    fprintf(stderr, "Failed to initialize OpenAL.\n");
    return;
  }

  /* Grab some contexts and things for audio. */
  if ((controller->context =
    alcCreateContext(controller->device, NULL)) == NULL) {
    fprintf(stderr, "Failed to create an OpenAL context.\n");

    alcCloseDevice(controller->device);
    controller->device = NULL;
    return;
  }

  alcMakeContextCurrent(controller->context);
  alGenBuffers(AUDIO_DMA_DEPTH, controller->buffers);
  alGenSources(1, &controller->source);

  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "Failed to generate OpenAL entities.\n");

    alDeleteSources(1, &controller->source);
    alDeleteBuffers(AUDIO_DMA_DEPTH, controller->buffers);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(controller->context);
    alcCloseDevice(controller->device);
    controller->device = NULL;
    return;
  }
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
    *data = 0;
    break;

  case AI_STATUS_REG:
    *data = 0x80000001;
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
    FIFOPush(controller);
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

