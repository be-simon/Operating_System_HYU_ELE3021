#include "types.h"
#include "stat.h"
#include "user.h"

int
main (void)
{
	int rc =  fork();
	if(rc < 0) { // fork fail
		printf(2, "fork failed\n");
		exit();
	} else if(rc == 0){ // child process
		while(1){
			printf(1, "Child\n");
			yield();
		}
	} else { // parent process
		while(1){
			printf(1, "Parent\n");
			yield();
		}
	}
}
