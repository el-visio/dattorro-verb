/*
Jon Dattorro reverb implementation
by el-visio 2022

The algorithm: https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf

This reverb is an fine-tuned IIR feedback delay network architecture
with lots of magic numbers involved (faithful to the original paper).

There are three basic components in the signal flow:
-delay lines
-all pass filters (keeps amplitude per frequency but messes with phase)
-low pass filters (pre-reverb filter and damping)

Mono input signal flow (function DattorroVerb_process):
1. Pre-delay
2. Input filter (low pass)
3. Input diffusor x 4 (all pass filter)
4. Signal splits into two halves of the "reverbation tank", for each:
   4.1 Cross feedback from post damping delay (4.6) of the other half
   4.2 Decay diffusor 1 (modulated all pass filter)
   4.3 Pre damping delay
   4.4 Damping (low pass filter)
   4.5 Decay diffusor 2 (all pass filter)
   4.6 Post damping delay

The final left / right signal is combined by tapping multiple
delay lines in the network, call functions DattorroVerb_getLeft 
and DattorroVerb_getRight for 100% wet stereo signal. 

My github:     https://github.com/el-visio
*/

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "verb.h"
#include "verb_structs.h"

#define MAX_PREDELAY 4800  // 100ms for 48k samplerate

/* Clamp value between min and max */
double clamp(double x, double min, double max)
{
  if (x < min)
    return min;
  
  if (x > max)
    return max;

  return x; 
}

/* Set delay amount */
void DelayBuffer_setDelay(DelayBuffer* db, uint16_t delay)
{
  db->readOffset = db->mask + 1 - delay;
}

/* Initialize DelayBuffer instance */
void DelayBuffer_init(DelayBuffer* db, uint16_t delay)
{
  uint16_t x;
  uint16_t numBits = 0;
  uint16_t bufferSize;

  memset(db, 0, sizeof(DelayBuffer));

  x = delay;

  // Calculate number of bits
  while (x) {
    numBits++;
    x >>= 1;
  }

  // Buffer size is always 2^n
  bufferSize = 1 << numBits;

  // Allocate buffer
  db->buffer = malloc(bufferSize * sizeof(double));
  if (!db->buffer)
    return;

  // Clear buffer
  memset(db->buffer, 0, bufferSize * sizeof(double));

  // Create bitmask for fast wrapping of the circular buffer
  db->mask = bufferSize - 1;
  DelayBuffer_setDelay(db, delay);
}

/* Free resources for DelayBuffer instance */
void DelayBuffer_delete(DelayBuffer* db)
{
  free(db->buffer);
  db->buffer = 0;
}

/* Write input value into buffer, read delayed output */
double DelayBuffer_process(DelayBuffer* db, uint16_t t, double in)
{
  db->buffer[t & db->mask] = in;
  return db->buffer[(t + db->readOffset) & db->mask];
}

/* Write value into delay buffer */
void DelayBuffer_write(DelayBuffer* db, uint16_t t, double in) 
{
  db->buffer[t & db->mask] = in;
}

/* Read delayed output value */
double DelayBuffer_read(DelayBuffer* db, uint16_t t) 
{
  return db->buffer[(t + db->readOffset) & db->mask];
}

/* Read delay from offset */
double DelayBuffer_readOffset(DelayBuffer* db, uint16_t t, uint16_t delay) 
{
  return db->buffer[(t + db->mask - delay + 1) & db->mask];
}

/* Apply all-pass filter */
double AllPassFilter_process(DelayBuffer* db, uint16_t t, double gain, double in)
{
  double delayed = DelayBuffer_read(db, t);
  in += delayed * -gain;
  DelayBuffer_write(db, t, in);
  return delayed + in * gain;
}

/* Apply Low pass filter */
double LowPassFilter_process(double* out, double freq, double in)
{
  *out += (in - *out) * freq;
  return *out;
}

/* Set pre-delay length (relative to MAX_PREDELAY) */
void DattorroVerb_setPreDelay(DattorroVerb* v, double value)
{
  DelayBuffer_setDelay(&v->preDelay, value * MAX_PREDELAY);
}

/* Set pre-filter amount */
void DattorroVerb_setPreFilter(struct sDattorroVerb* v, double value)
{
  v->preFilterAmount = value;
}

/* Set input diffusion 1 amount */
void DattorroVerb_setInputDiffusion1(struct sDattorroVerb* v, double value)
{
  v->inputDiffusion1Amount = value;
}

/* Set input diffusion 2 amount */
void DattorroVerb_setInputDiffusion2(struct sDattorroVerb* v, double value)
{
  v->inputDiffusion2Amount = value;
}

/* Set decay diffusion 1 amount */
void DattorroVerb_setDecayDiffusion(struct sDattorroVerb* v, double value)
{
  v->decayDiffusion1Amount = value;
}

/* Set decay amount and calculate related decay diffusion 2 amount */
void DattorroVerb_setDecay(DattorroVerb* v, double value)
{
  v->decayAmount = value;
  v->decayDiffusion2Amount = clamp(value + 0.15, 0.25, 0.50);
}

/* Set damping amount */
void DattorroVerb_setDamping(struct sDattorroVerb* v, double value)
{
  v->dampingAmount = value;
}

/* Initialize DattorroVerb instance */
void initialize(DattorroVerb* v)
{
  memset(v, 0, sizeof(DattorroVerb));

  // Init delay buffers using Jon Dattorro's magic numbers
  DelayBuffer_init(&v->preDelay, MAX_PREDELAY);

  DelayBuffer_init(&v->inDiffusion[0], 142);
  DelayBuffer_init(&v->inDiffusion[1], 107);
  DelayBuffer_init(&v->inDiffusion[2], 379);
  DelayBuffer_init(&v->inDiffusion[3], 277);

  DelayBuffer_init(&v->decayDiffusion1[0], 672);    // + EXCURSION
  DelayBuffer_init(&v->preDampingDelay[0], 4453);
  DelayBuffer_init(&v->decayDiffusion2[0], 1800);
  DelayBuffer_init(&v->postDampingDelay[0], 3720);

  DelayBuffer_init(&v->decayDiffusion1[1], 908);    // + EXCURSION
  DelayBuffer_init(&v->preDampingDelay[1], 4217);
  DelayBuffer_init(&v->decayDiffusion2[1], 2656);
  DelayBuffer_init(&v->postDampingDelay[1], 3163);

  // Default settings
  DattorroVerb_setPreDelay(v, 0.1);
  DattorroVerb_setPreFilter(v, 0.85);
  DattorroVerb_setInputDiffusion1(v, 0.75);
  DattorroVerb_setInputDiffusion2(v, 0.625);
  DattorroVerb_setDecay(v, 0.75);
  DattorroVerb_setDecayDiffusion(v, 0.70);
  DattorroVerb_setDamping(v, 0.95);
}

/* Get pointer to initialized DattorroVerb instance */
DattorroVerb* DattorroVerb_create(void)
{
  DattorroVerb* v = malloc(sizeof(DattorroVerb));
  if (!v)
    return NULL;

  initialize(v);
  return v;
}

/* Free resources and delete DattorroVerb instance */
void DattorroVerb_delete(DattorroVerb* v)
{
  // Free delay buffers
  DelayBuffer_delete(&v->preDelay);

  for (int i = 0; i < 4; i++) {
    DelayBuffer_delete(&v->inDiffusion[i]);
  }

  for (int i = 0; i < 2; i++) {
    DelayBuffer_delete(&v->decayDiffusion1[i]);
    DelayBuffer_delete(&v->preDampingDelay[i]);
    DelayBuffer_delete(&v->decayDiffusion2[i]);
    DelayBuffer_delete(&v->postDampingDelay[i]);
  }

  free(v);
}

// Process mono audio
// 
// After calling this function you can
// get wet stereo reverb signal by calling
// DattorroVerb_getLeft and DattorroVerb_getRight 
void DattorroVerb_process(DattorroVerb* v, double in)
{
  double x, x1;

  // Modulate decayDiffusion1A & decayDiffusion1B
  if ((v->t & 0x07ff) == 0) {
    if (v->t < (1<<15)) {
      v->decayDiffusion1[0].readOffset--;
      v->decayDiffusion1[1].readOffset--;
    } else {
      v->decayDiffusion1[0].readOffset++;
      v->decayDiffusion1[1].readOffset++;
    }
  }

  // Pre-delay
  x = DelayBuffer_process(&v->preDelay, v->t, in);

  // Pre-filter
  x = LowPassFilter_process(&v->preFilter, v->preFilterAmount, x);

  // Input diffusion
  for (int i = 0; i < 4; i++) {
    x = AllPassFilter_process(&v->inDiffusion[i], 
                              v->t, 
                              (i < 2) ? v->inputDiffusion1Amount : v->inputDiffusion2Amount, 
                              x);
  }

  for (int i = 0; i < 2; i++) {
    // Add cross feedback
    x1 = x + DelayBuffer_read(&v->postDampingDelay[1 - i], v->t) * v->decayAmount;

    // Process single half of the tank
    x1 = AllPassFilter_process(&v->decayDiffusion1[i], v->t, -v->decayDiffusion1Amount, x1);
    x1 = DelayBuffer_process(&v->preDampingDelay[i], v->t, x1);
    x1 = LowPassFilter_process(&v->damping[i], v->dampingAmount, x1);
    x1 *= v->decayAmount;
    x1 = AllPassFilter_process(&v->decayDiffusion2[i], v->t, v->decayDiffusion2Amount, x1);
    DelayBuffer_write(&v->postDampingDelay[i], v->t, x1);
  }

  // Increment delay position
  v->t++;
}

// Get left channel reverb
double DattorroVerb_getLeft(DattorroVerb* v)
{
  double a;
  a  = DelayBuffer_readOffset(&v->preDampingDelay[1],  v->t, 266);
  a += DelayBuffer_readOffset(&v->preDampingDelay[1],  v->t, 2974);
  a -= DelayBuffer_readOffset(&v->decayDiffusion2[1],  v->t, 1913);
  a += DelayBuffer_readOffset(&v->postDampingDelay[1], v->t, 1996);
  a -= DelayBuffer_readOffset(&v->preDampingDelay[0],  v->t, 1990);
  a -= DelayBuffer_readOffset(&v->decayDiffusion2[0],  v->t, 187);
  a += DelayBuffer_readOffset(&v->postDampingDelay[0], v->t, 1066);
  return a;
}

// Get right channel reverb
double DattorroVerb_getRight(DattorroVerb* v)
{
  double a;
  a  = DelayBuffer_readOffset(&v->preDampingDelay[0],  v->t, 353);
  a += DelayBuffer_readOffset(&v->preDampingDelay[0],  v->t, 3627);
  a -= DelayBuffer_readOffset(&v->decayDiffusion2[0],  v->t, 1228);
  a += DelayBuffer_readOffset(&v->postDampingDelay[0], v->t, 2673);
  a -= DelayBuffer_readOffset(&v->preDampingDelay[1],  v->t, 2111);
  a -= DelayBuffer_readOffset(&v->decayDiffusion2[1],  v->t, 335);
  a += DelayBuffer_readOffset(&v->postDampingDelay[1], v->t, 121);
  return a;
}
