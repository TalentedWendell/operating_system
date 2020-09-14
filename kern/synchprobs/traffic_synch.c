#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;


static struct lock *lockinter;
//all possible ways for threads
static struct cv *n;
static struct cv *s;
static struct cv *e;
static struct cv *w;

//static int firstposition = -1; //-1 means empty, 0 means north, 1 means south, 2 means east, 3 meanswest.
//static int secondposition = -1; // same as firstpostition. note cars in the intersection will always try to occupy first position then second position. if the car at the position is leaving, we move the car in the second position to the first one. 
static int count = 0; //the variable count keeps track of how many cars there are in the intersection. if count == 2, this could suggest that both cars in the intersection are from the same direction.
static int waitlist[4]; //position 0 indicates how many cars are waiting from the north. 
static int entering = -1; // indicates it is whose turn to enter the intersection. -1 indicates no waitlist is entering. 
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  //intersectionSem = sem_create("intersectionSem",1);
  //if (intersectionSem == NULL) {
  //  panic("could not create intersection semaphore");
  //}
  
  lockinter = lock_create("lockinter");
  
  n = cv_create("n");
  s = cv_create("s");
  e = cv_create("e");
  w = cv_create("w");
  for(int i = 0; i < 4; i++){
      waitlist[i] = 0;
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);
  

  KASSERT(lockinter != NULL);

  KASSERT(n != NULL);
  KASSERT(s != NULL);
  KASSERT(e != NULL);
  KASSERT(w != NULL);

  lock_destroy(lockinter);
  cv_destroy(n);
  cv_destroy(s);
  cv_destroy(e);
  cv_destroy(w);
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //P(intersectionSem);
  

  (void)destination;

  KASSERT(lockinter != NULL);

  KASSERT(n != NULL);
  KASSERT(s != NULL);
  KASSERT(e != NULL);
  KASSERT(w != NULL);
  
  
  lock_acquire(lockinter);
  //bool godirectly = true;
  if(origin == north){
          if(entering == -1){
              entering = 0;
              count = 1;
              lock_release(lockinter);
              return;
          } else {
              if(entering != 0){
                  waitlist[0]++;
                  //godirectly = false;
                  cv_wait(n, lockinter);
              }
              //if(godirectly == false){
              //    waitlist[0]--;
              //}
              waitlist[0] = 0;
          }
  } else if(origin == south){
          if(entering == -1){
              entering = 1;
              count = 1;
              lock_release(lockinter);
              return;
          } else {
              if(entering != 1){
                  waitlist[1]++;
                  //godirectly = false;
                  cv_wait(s, lockinter);
              }
              //if(godirectly == false){
              //    waitlist[1]--;
              //}
              waitlist[1] = 0;
          }   
  } else if(origin == east){

          if(entering == -1){
              entering = 2;
              count = 1;
              lock_release(lockinter);
              return;
          } else {
              if(entering != 2){
                  waitlist[2]++;
                  //godirectly = false;
                  cv_wait(e, lockinter);
              }
              //if(godirectly == false){
              //    waitlist[2]--;
              //}
              waitlist[2] = 0;
          }
      
  } else {
          if(entering == -1){
              entering = 3;
              count = 1;
              lock_release(lockinter);
              return;
          } else {
              if(entering != 3){
                  waitlist[3]++;
                  //godirectly = false;
                  cv_wait(w, lockinter);
              }
              //if(godirectly == false){
              //    waitlist[3]--;
              //}
              waitlist[3] = 0;
          }
  }
  count++;
  lock_release(lockinter);
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);
  
  (void)destination;
  (void)origin;
  KASSERT(lockinter != NULL);

  KASSERT(n != NULL);
  KASSERT(s != NULL);
  KASSERT(e != NULL);
  KASSERT(w != NULL);

  
  lock_acquire(lockinter);
  count = count - 1;
  if((count == 0) && (waitlist[0] == 0) && (waitlist[1] == 0) && (waitlist[2] == 0) && (waitlist[3] == 0)){
      entering = -1;
      lock_release(lockinter);
      return;
  }
  if(count == 0){
      if(entering == 3){
          entering = 0;
      } else {
          entering++;
      }
      while(waitlist[entering] == 0){
          if(entering == 3){
              entering = 0;
          } else {
              entering++;
          }
      }
      if(entering == 0){
          cv_broadcast(n, lockinter);
      } else if(entering == 1){
          cv_broadcast(s, lockinter);
      } else if(entering == 2){
          cv_broadcast(e, lockinter);
      } else {
          cv_broadcast(w, lockinter);
      }
  }
  lock_release(lockinter);
  return;
}
