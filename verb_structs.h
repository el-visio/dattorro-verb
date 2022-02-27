#include <stdint.h>

enum {
  TAP_MAIN = 0,
  TAP_OUT1,
  TAP_OUT2,
  TAP_OUT3,
  MAX_TAPS
};

/* DelayBuffer context, also used in AllPassFilter */
typedef struct sDelayBuffer {
  // Sample buffer
  double* buffer;

  // Mask for fast array index wrapping in read / write 
  uint16_t mask;

  // Read offsets
  uint16_t readOffset[MAX_TAPS];
} DelayBuffer;

/* DattorroVerb context */
typedef struct sDattorroVerb {
  // -- Reverb feedback network components --

  // Pre-delay
  DelayBuffer preDelay;       // Delay

  // Pre-filter
  double      preFilter;      // LPF

  // input diffusors
  DelayBuffer inDiffusion[4]; // APF

  // Reverbation tank left / right halves
  DelayBuffer decayDiffusion1[2];  // APF
  DelayBuffer preDampingDelay[2];  // Delay
  double      damping[2];          // LPF
  DelayBuffer decayDiffusion2[2];  // APF
  DelayBuffer postDampingDelay[2]; // Delay

  // -- Reverb settings --
  double   preFilterAmount;

  double   inputDiffusion1Amount;
  double   inputDiffusion2Amount;

  double   decayDiffusion1Amount;
  double   dampingAmount;
  double   decayAmount;
  double   decayDiffusion2Amount;  // Automatically set in DattorroVerb_setDecay

  // Cycle count for syncing delay lines
  uint16_t t;
} DattorroVerb;
