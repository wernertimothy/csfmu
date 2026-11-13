#ifndef TYPED_DEMO_VARIABLES_H
#define TYPED_DEMO_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double rate;       /* ramp rate */
    int    direction;  /* +1 ramps up, -1 ramps down */
    bool   clamp;      /* limit the output y to the value of limit */
    double limit;      /* threshold of the ramp */
} Params;

/* inputs */
typedef struct {
    bool enable;       /* ramp runs while true */
    int  boost;        /* multiplier of the ramp rate */
} Inputs;

/* outputs */
typedef struct {
    double y;              /* ramp value */
    int    level;          /* ramp value rounded down */
    bool   limit_reached;  /* true once the ramp has reached the limit */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;          /* ramp value */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void typedDemoSystem(modelInstance* model);
/* output equation */
void typedDemoOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
