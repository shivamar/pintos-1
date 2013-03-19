/* sort.c 

   Test program to sort a large number of integers.
 
   Intention is to stress virtual memory system.
 
   Ideally, we could read the unsorted array off of the file
   system, and store the result back to the file system! */
#include <stdio.h>
#include <random.h>

/* Size of array to sort. */
#define SORT_SIZE 128
#define MAX_ELEM 1024

int
main (void)
{
  /* Array to sort.  Static to reduce stack usage. */
  static int array[SORT_SIZE];

  int i, j, tmp;

  random_init (0);
  /* First initialize the array in descending order. */
  for (i = 0; i < SORT_SIZE; i++)
    array[i] = (int)(random_ulong() % MAX_ELEM);

  printf ("before sort we have:");
  for (i = 0; i < 10; ++i)
    printf ("%d ", array[i]);
  printf ("...\n");

  /* Then sort in ascending order. */
  for (i = 0; i < SORT_SIZE - 1; i++)
    for (j = 0; j < SORT_SIZE - 1 - i; j++)
      if (array[j] > array[j + 1])
	{
	  tmp = array[j];
	  array[j] = array[j + 1];
	  array[j + 1] = tmp;
	}

  printf ("after sort we have:");
  for (i = 0; i < 10; ++i)
    printf ("%d ", array[i]);
  printf ("...\n");

  printf ("sort exiting with code %d\n", array[0]);
  return array[0];
}
