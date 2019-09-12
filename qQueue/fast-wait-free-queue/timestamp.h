typedef struct __attribute__((aligned(64))) timestamp
{
  long long invoked;
  char type[128];
  int val;
  long key;
  int threadId;
} timestamp;