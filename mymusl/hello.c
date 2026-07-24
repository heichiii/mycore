#include <stdio.h>
#include <sched.h>
int main()
{
	printf("hello:Hello World!\n");
	printf("hello:Before yield.\n");
	int y=sched_yield();
	printf("hello:After yield, status:%d\n",y);
	return 0;
}
