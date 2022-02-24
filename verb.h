struct sDattorroVerb;

void DattorroVerb_setPreDelay(struct sDattorroVerb* v, double value);

void DattorroVerb_setDecay(struct sDattorroVerb* v, double value);

struct sDattorroVerb* DattorroVerb_create(void);

void DattorroVerb_delete(struct sDattorroVerb* v);

void DattorroVerb_process(struct sDattorroVerb* v, double in);

double DattorroVerb_getLeft(struct sDattorroVerb* v);

double DattorroVerb_getRight(struct sDattorroVerb* v);
