#include "testutils.h"
#include "test.h"

int operations_done = 0;

void *test(void *data) {
  int unext = 0, last = -1; 
  val_t val = 0;
	
  thread_data_t *d = (thread_data_t *)data;
	
  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  while (1) {
      int thisindex = atomic_fetch_add(&operations_done, 1);
      if (thisindex > d->add_operations + d->del_operations) {
		break;
      }
			
    if (unext) { // update
				
      if (last < 0) { // add
					
	val = rand_range_re(&d->seed, d->range);
	if (set_add_l(d->set, val, TRANSACTIONAL)) {
	  d->nb_added++;
	  last = val;
	} 				
	d->nb_add++;
					
      } else { // remove
					
	  val = rand_range_re(&d->seed, d->range);
	  if (set_remove_l(d->set, val, TRANSACTIONAL)) {
	    d->nb_removed++;
	    last = -1;
	  } 
					
	d->nb_remove++;
      }
				
    } else { // read
				
      val = rand_range_re(&d->seed, d->range);
				
      if (set_contains_l(d->set, val, TRANSACTIONAL)) 
	d->nb_found++;
      d->nb_contains++;			
    }
			
    /* Is the next op an update? */
    unext = (rand_range_re(&d->seed, 100) - 1 < (double)d->del_operations / (d->add_operations + d->del_operations));
			
  }	
  return NULL;
}
