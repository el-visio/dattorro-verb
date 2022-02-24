# dattorro-verb
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
