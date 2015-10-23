#include "misc.h"

/* <doc>
 * void * SafeMalloc(size_t size)
 * mallocs new memory.  If malloc fails, prints error message and terminates program.
 *
 * </doc>
 */ 
void * SafeMalloc(size_t size) {
  void * result;

  if ( (result = malloc(size)) ) {
    return(result);
  } else {
    printf("memory overflow: malloc failed in SafeMalloc.");
    printf("  Exiting Program.\n");
    exit(-1);
    return(0);
  }
}

/* <doc>
 * void NullFunction(void * junk)
 * NullFunction does nothing it is included so that it can be passed
 * as a function to RBTreeCreate when no other suitable function has
 * been defined
 * </doc>
 */
void NullFunction(void * junk) { ; }

