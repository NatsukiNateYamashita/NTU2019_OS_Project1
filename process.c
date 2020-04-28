#define _GNU_SOURCE
#include "process.h"
#include <errno.h>
#include <linux/kernel.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#define GETTIMEOFDAY 96
#define MYPRINTK 333

#define FIFO	1
#define RR	2
#define SJF	3
#define PSJF	4

#define debug 0

// https://linuxjm.osdn.jp/html/LDP_man-pages/man3/CPU_SET.3.html
// https://linuxjm.osdn.jp/html/LDP_man-pages/man2/sched_setaffinity.2.html
int mask_cpu(int pid, int core)
{	
	
	if (core > sizeof(cpu_set_t)) {
		printf("There is no core you wanna use");
		return -1;
	}
	
	cpu_set_t mask;
	CPU_ZERO(&mask);
	// printf("core: %d\n", core);
	CPU_SET(core, &mask);
	if (sched_setaffinity(pid, sizeof(mask), &mask) < 0) {
		printf("ERROR: sched_setaffinity");
		exit(1);
	}
	return 0;
}
// https://kaworu.jpn.org/c/%E3%83%97%E3%83%AD%E3%82%BB%E3%82%B9%E3%81%AE%E4%BD%9C%E6%88%90_fork
int execute(struct process proc) {
	int pid = fork();
	if (debug) {
		printf("### proc_exe ###\n");
		// printf("[proc_exe proc.pid] %d\n", proc.pid);
		printf("[proc_exe pid] %d\n", pid);
	}
	// フォークミス
	if (pid < 0) {
		printf("fork error");
		return -1;
	}
	// フォークOK
	if (pid == 0) {
		// 開始時間取得
		struct timeval start_tv, end_tv;
		syscall(GETTIMEOFDAY, &start_tv, NULL);

		if (debug) {
			printf("[GETTIMEOFDAY] start_tv.tv_sec: %lu  start_tv.tv_usec: %lu\n", start_tv.tv_sec, start_tv.tv_usec);
		}
		// タスク実行
		for (int i = 0; i < proc.t_exec; i++) {
			volatile unsigned long i;		
			for (i = 0; i < 1000000UL; i++);

			if (debug) {
				if (i % 100 == 0)
					printf("%s: %lu/%d\n", proc.name, i, proc.t_exec);
			}
		}
		// 終了時間取得
		syscall(GETTIMEOFDAY, &end_tv, NULL);
		// PRINTK
		syscall(MYPRINTK, getpid(), start_tv.tv_sec, start_tv.tv_usec, end_tv.tv_sec, end_tv.tv_usec);
		
		if (debug){
			printf("[GETTIMEOFDAY] end_tv.tv_sec: %lu  end_tv.tv_usec:%lu\n", end_tv.tv_sec, end_tv.tv_usec);
			printf("[project1] %d %lu.%09lu %lu.%09lu\n", getpid(),  start_tv.tv_sec, start_tv.tv_usec, end_tv.tv_sec, end_tv.tv_usec);
		}	
		
		exit(0);
	}
	
	// 子プロセスの為にCPUを確保
	mask_cpu(pid, CHILD_CPU);
	
	if (debug) {
		// printf("[CHILD proc_exe proc.pid] %d\n", proc.pid);
		printf("[CHILD proc_exe pid] %d\n", pid);
		printf("################\n");
	}

	return pid;
}

int priority_idling(int pid)
{
	// https://docs.oracle.com/cd/E19455-01/806-2732/attrib-11564/index.html
	struct sched_param param;
	param.sched_priority = 0;
	// https://linuxjm.osdn.jp/html/LDP_man-pages/man2/sched_setscheduler.2.html
	int ret = sched_setscheduler(pid, SCHED_IDLE, &param);
	if (ret < 0) {
		ret = sched_setscheduler(pid, SCHED_RR, &param);
		if (ret < 0) {
			perror("ERROR: sched_setscheduler IDLE\n");
			return -1;
		}
	}
	return ret;
}

int prioritize(int pid)
{
	// https://docs.oracle.com/cd/E19455-01/806-2732/attrib-11564/index.html
	struct sched_param param;
	param.sched_priority = 0;
	// https://linuxjm.osdn.jp/html/LDP_man-pages/man2/sched_setscheduler.2.html
	
	int ret = sched_setscheduler(pid, SCHED_OTHER, &param);
	if (ret < 0) {
		ret = sched_setscheduler(pid, SCHED_RR, &param);
		if (ret < 0) {
			perror("ERROR: sched_setscheduler OTHER\n");
			return -1;
		}
	}
	return ret;
}
