#define ARG_FORMAT      "%d;%d;%d;%d"
#define LO(x)           ((int) ((unsigned long) (x)))
#define HI(x)           ((int) ((unsigned long) (x)) >> 16)
#define JOIN(lo, hi)    ((unsigned long) ((((int) (hi)) << 16) | (int) (lo)))
#define ROUTINE_ORDER   0x00009F00   /* invoke before shutting down PM */
