/*
Jon Dattorro reverb implementation
by el-visio 2022

The algorithm: https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf
My github:     https://github.com/el-visio
*/

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "verb.h"

#define MAX_PREDELAY 4800  // 100ms for 48k samplerate

typedef struct sDelayBuffer {
  double* buffer;
  uint16_t mask;
  uint16_t readOffset;
} DelayBuffer;

typedef struct sDattorroVerb {
  // LPFs, APFs and delay lines

  // Pre-delay
  DelayBuffer preDelay;          // Delay

  // Pre-filter
  double      preFilter;         // LPF

  // input diffusors
  DelayBuffer inDiffusion1;      // APF
  DelayBuffer inDiffusion2;      // APF
  DelayBuffer inDiffusion3;      // APF
  DelayBuffer inDiffusion4;      // APF

  // Left side of the tank
  DelayBuffer decayDiffusion1A;  // APF
  DelayBuffer preDampingDelayA;  // Delay
  double      dampingA;          // LPF
  DelayBuffer decayDiffusion2A;  // APF
  DelayBuffer postDampingDelayA; // Delay

  // Right side of the tank
  DelayBuffer decayDiffusion1B;  // APF
  DelayBuffer preDampingDelayB;  // Delay
  double      dampingB;          // LPF
  DelayBuffer decayDiffusion2B;  // APF
  DelayBuffer postDampingDelayB; // Delay

  // Reverb settings
  double   preFilterAmount;

  double   inputDiffusion1Amount;
  double   inputDiffusion2Amount;

  double   decayDiffusion1Amount;
  double   dampingAmount;
  double   decayAmount;
  double   decayDiffusion2Amount;

  uint16_t t;
} DattorroVerb;


double clamp(double x, double min, double max)
{
  if (x < min)
    return min;
  
  if (x > max)
    return max;

  return x; 
}

void DelayBuffer_setDelay(DelayBuffer* db, uint16_t delay)
{
  if (delay > db->mask)
    return;

  db->readOffset = db->mask + 1 - delay;
}

void DelayBuffer_init(DelayBuffer* db, uint16_t delay)
{
  uint16_t x;
  uint16_t numBits = 0;
  uint16_t bufferSize;

  memset(db, 0, sizeof(DelayBuffer));

  x = delay;

  while (x) {
    numBits++;
    x >>= 1;
  }

  bufferSize = 1 << numBits;

  db->buffer = malloc(bufferSize * sizeof(double));
  if (!db->buffer)
    return;

  memset(db->buffer, 0, bufferSize * sizeof(double));

  db->mask = bufferSize - 1;
  DelayBuffer_setDelay(db, delay);
}

void DelayBuffer_delete(DelayBuffer* db)
{
  free(db->buffer);
  memset(db, 0, sizeof(DelayBuffer));
}

double DelayBuffer_process(DelayBuffer* db, uint16_t t, double in)
{
  db->buffer[t & db->mask] = in;
  return db->buffer[(t + db->readOffset) & db->mask];
}

void DelayBuffer_write(DelayBuffer* db, uint16_t t, double in) 
{
  db->buffer[t & db->mask] = in;
}

double DelayBuffer_read(DelayBuffer* db, uint16_t t) 
{
  return db->buffer[(t + db->readOffset) & db->mask];
}

double DelayBuffer_readAt(DelayBuffer* db, uint16_t t, uint16_t delay) 
{
  return db->buffer[(t + db->mask - delay + 1) & db->mask];
}

double AllPassFilter_process(DelayBuffer* db, uint16_t t, double gain, double in)
{
  double delayed = DelayBuffer_read(db, t);
  in += delayed * -gain;
  DelayBuffer_write(db, t, in);
  return delayed + in * gain;
}

double LowPassFilter_process(double* out, double freq, double in)
{
  *out += (in - *out) * freq;
  return *out;
}

void DattorroVerb_setPreDelay(DattorroVerb* v, double value)
{
  DelayBuffer_setDelay(&v->preDelay, value * MAX_PREDELAY);
}

void DattorroVerb_setDecay(DattorroVerb* v, double value)
{
  v->decayAmount = value;
  v->decayDiffusion2Amount = clamp(value + 0.15, 0.25, 0.50);
}

void DattorroVerb_init(DattorroVerb* v)
{
  memset(v, 0, sizeof(DattorroVerb));

  // Init delay buffers
  DelayBuffer_init(&v->preDelay, MAX_PREDELAY);

  DelayBuffer_init(&v->inDiffusion1, 142);
  DelayBuffer_init(&v->inDiffusion2, 107);
  DelayBuffer_init(&v->inDiffusion3, 379);
  DelayBuffer_init(&v->inDiffusion4, 277);

  DelayBuffer_init(&v->decayDiffusion1A, 672);    // + EXCURSION
  DelayBuffer_init(&v->preDampingDelayA, 4453);
  DelayBuffer_init(&v->decayDiffusion2A, 1800);
  DelayBuffer_init(&v->postDampingDelayA, 3720);

  DelayBuffer_init(&v->decayDiffusion1B, 908);    // + EXCURSION
  DelayBuffer_init(&v->preDampingDelayB, 4217);
  DelayBuffer_init(&v->decayDiffusion2B, 2656);
  DelayBuffer_init(&v->postDampingDelayB, 3163);

  // Default settings
  DattorroVerb_setPreDelay(v, 0.1);
  v->preFilterAmount = 0.85;
  v->inputDiffusion1Amount = 0.75;
  v->inputDiffusion2Amount = 0.625;
  DattorroVerb_setDecay(v, 0.75);
  v->decayDiffusion1Amount = 0.70;
  v->dampingAmount = 0.95;
}

DattorroVerb* DattorroVerb_create(void)
{
  DattorroVerb* v = malloc(sizeof(DattorroVerb));
  if (!v)
    return NULL;

  DattorroVerb_init(v);
  return v;
}

void DattorroVerb_delete(DattorroVerb* v)
{
  // Free delay buffers
  DelayBuffer_delete(&v->preDelay);

  DelayBuffer_delete(&v->inDiffusion1);
  DelayBuffer_delete(&v->inDiffusion2);
  DelayBuffer_delete(&v->inDiffusion3);
  DelayBuffer_delete(&v->inDiffusion4);

  DelayBuffer_delete(&v->decayDiffusion1A);
  DelayBuffer_delete(&v->preDampingDelayA);
  DelayBuffer_delete(&v->decayDiffusion2A);
  DelayBuffer_delete(&v->postDampingDelayA);

  DelayBuffer_delete(&v->decayDiffusion1B);
  DelayBuffer_delete(&v->preDampingDelayB);
  DelayBuffer_delete(&v->decayDiffusion2B);
  DelayBuffer_delete(&v->postDampingDelayB);
}

// Process mono audio
// 
// After calling this function you can
// get wet stereo reverb signal by calling
// DattorroVerb_getLeft and DattorroVerb_getRight 
void DattorroVerb_process(DattorroVerb* v, double in)
{
  double x, x1, x2;

  // Modulate decayDiffusion1A & decayDiffusion1B
  if ((v->t & 0x07ff) == 0) {
    if (v->t < (1<<15)) {
      v->decayDiffusion1A.readOffset--;
      v->decayDiffusion1B.readOffset--;
    } else {
      v->decayDiffusion1A.readOffset++;
      v->decayDiffusion1B.readOffset++;
    }
  }

  // Pre-delay
  x = DelayBuffer_process(&v->preDelay, v->t, in);

  // Pre-filter
  x = LowPassFilter_process(&v->preFilter, v->preFilterAmount, x);

  // Input diffusion
  x = AllPassFilter_process(&v->inDiffusion1, v->t, v->inputDiffusion1Amount, x);
  x = AllPassFilter_process(&v->inDiffusion2, v->t, v->inputDiffusion1Amount, x);
  x = AllPassFilter_process(&v->inDiffusion3, v->t, v->inputDiffusion2Amount, x);
  x = AllPassFilter_process(&v->inDiffusion4, v->t, v->inputDiffusion2Amount, x);

  // Add cross feedback
  x1 = x + DelayBuffer_read(&v->postDampingDelayB, v->t) * v->decayAmount;
  x2 = x + DelayBuffer_read(&v->postDampingDelayA, v->t) * v->decayAmount;

  // Left side of the tank
  x1 = AllPassFilter_process(&v->decayDiffusion1A, v->t, -v->decayDiffusion1Amount, x1);
  x1 = DelayBuffer_process(&v->preDampingDelayA, v->t, x1);
  x1 = LowPassFilter_process(&v->dampingA, v->dampingAmount, x1);
  x1 *= v->decayAmount;
  x1 = AllPassFilter_process(&v->decayDiffusion2A, v->t, v->decayDiffusion2Amount, x1);
  DelayBuffer_write(&v->postDampingDelayA, v->t, x1);

  // Right side of the tank
  x2 = AllPassFilter_process(&v->decayDiffusion1B, v->t, -v->decayDiffusion1Amount, x2);
  x2 = DelayBuffer_process(&v->preDampingDelayB, v->t, x2);
  x2 = LowPassFilter_process(&v->dampingB, v->dampingAmount, x2);
  x2 *= v->decayAmount;
  x2 = AllPassFilter_process(&v->decayDiffusion2B, v->t, v->decayDiffusion2Amount, x2);
  DelayBuffer_write(&v->postDampingDelayB, v->t, x2);

  // Increment delay position
  v->t++;
}

// Get left channel reverb
double DattorroVerb_getLeft(DattorroVerb* v)
{
  double a;
  a  = DelayBuffer_readAt(&v->preDampingDelayB,  v->t, 266);
  a += DelayBuffer_readAt(&v->preDampingDelayB,  v->t, 2974);
  a -= DelayBuffer_readAt(&v->decayDiffusion2B,  v->t, 1913);
  a += DelayBuffer_readAt(&v->postDampingDelayB, v->t, 1996);
  a -= DelayBuffer_readAt(&v->preDampingDelayA,  v->t, 1990);
  a -= DelayBuffer_readAt(&v->decayDiffusion2A,  v->t, 187);
  a += DelayBuffer_readAt(&v->postDampingDelayA, v->t, 1066);
  return a;
}

// Get right channel reverb
double DattorroVerb_getRight(DattorroVerb* v)
{
  double a;
  a  = DelayBuffer_readAt(&v->preDampingDelayA,  v->t, 353);
  a += DelayBuffer_readAt(&v->preDampingDelayA,  v->t, 3627);
  a -= DelayBuffer_readAt(&v->decayDiffusion2A,  v->t, 1228);
  a += DelayBuffer_readAt(&v->postDampingDelayA, v->t, 2673);
  a -= DelayBuffer_readAt(&v->preDampingDelayB,  v->t, 2111);
  a -= DelayBuffer_readAt(&v->decayDiffusion2B,  v->t, 335);
  a += DelayBuffer_readAt(&v->postDampingDelayB, v->t, 121);
  return a;
}
