#ifdef __STDC__
#include <limits.h>
#else
#include <assert.h>
#endif

int
main()
{
#if defined __stub_tzset || defined __stub___tzset
  return 0;
#else
this system have stub
  return 0;
#endif
}
