#include <stdio.h>
#include <sched.h>
#include <limits.h>
int main()
{
	int a = 10;
	printf("hello:Hello World!\n");
	printf("hello:Before loop.\n");
	while (a>0)
	{
		a--;
		printf("hello:a=%d\n",a);
		sched_yield();
	}
	
	printf("hello:Finish loop.\n");

	return 0;
}
