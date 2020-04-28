#ifndef _PROCESS_H_
#define _PROCESS_H_
#include <sys/types.h>
// mask_cpu用
#define CHILD_CPU 1
#define PARENT_CPU 0

struct process {
	char name[32];
	int t_ready;
	int t_exec;
	pid_t pid;
};

int mask_cpu(int pid, int core);
int execute(struct process proc);
int priority_idling(int pid);
int prioritize(int pid);

#endif
