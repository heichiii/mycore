#include <stdio.h>
#include <sched.h>
#include <limits.h>
int main()
{
	int a = INT_MAX/40;
	printf("hello:Hello World!\n");
	printf("hello:Before loop.\n");
	while (a>0)
	{
		a--;
	}
	
	printf("hello:Finish loop.\n");

	return 0;
}
