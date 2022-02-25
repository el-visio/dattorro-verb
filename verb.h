struct sDattorroVerb;

/* Get pointer to initialized DattorroVerb struct */
struct sDattorroVerb* DattorroVerb_create(void);

/* Free resources and delete DattorroVerb instance */
void DattorroVerb_delete(struct sDattorroVerb* v);

/* Set reverb parameters */
void DattorroVerb_setPreDelay(struct sDattorroVerb* v, double value);
void DattorroVerb_setPreFilter(struct sDattorroVerb* v, double value);
void DattorroVerb_setInputDiffusion1(struct sDattorroVerb* v, double value);
void DattorroVerb_setInputDiffusion2(struct sDattorroVerb* v, double value);
void DattorroVerb_setDecayDiffusion(struct sDattorroVerb* v, double value);
void DattorroVerb_setDecay(struct sDattorroVerb* v, double value);
void DattorroVerb_setDamping(struct sDattorroVerb* v, double value);

/* Send mono input into reverbation tank */
void DattorroVerb_process(struct sDattorroVerb* v, double in);

/* Get reverbated signal for left channel */
double DattorroVerb_getLeft(struct sDattorroVerb* v);

/* Get reverbated signal for right channel */
double DattorroVerb_getRight(struct sDattorroVerb* v);
