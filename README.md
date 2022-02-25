# dattorro-verb
Jon Dattorro reverb implementation in C
(by El Visio 2022)

The algorithm: https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf

This reverb is a fine-tuned IIR feedback delay network with lots of magic numbers involved (faithful to the original paper).

The basic components used to implement the network:
- delay lines
- all pass filters (keeps amplitude per frequency but messes with phase)
- low pass filters (pre-reverb filter and damping)

Input signal flow (function DattorroVerb_process):
1. Pre-delay
2. Input filter (low pass)
3. Input diffusor x 4 (all pass filter)
4. Signal splits into two halves of the "reverbation tank", for each:
   1. Cross feedback from post damping delay (vi) of the other half
   2. Decay diffusor 1 (modulated all pass filter)
   3. Pre damping delay
   4. Damping (low pass filter)
   5. Decay diffusor 2 (all pass filter)
   6. Post damping delay

The final reverbated output is calculated by tapping multiple delay lines in the network. 

Call functions DattorroVerb_getLeft and DattorroVerb_getRight for 100% wet stereo signal. 
