#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "process.h"

#define FIFO	1
#define RR	2
#define SJF	3
#define PSJF	4

#define debug 0

int cmp(const void *a, const void *b){
	struct process *A, *B;
	A = (struct process *)a;
	B = (struct process *)b;
	if(A->t_ready < B->t_ready)
		return -1;
	if(A->t_ready > B->t_ready)
		return 0;
	// if readyT is equal, compare pid
	if(A->pid < B->pid)
		return -1;
	if(A->pid == B->pid)
		return 0;
	    

}

int main(int argc, char* argv[]) {
//////////////////////////////////////////////////////////////////////////////////////////////////// INPUT
	char sched_policy[256];
	int nproc;
	struct process *proc;

	scanf("%s", sched_policy);
	scanf("%d", &nproc);

	proc = (struct process *)malloc(sizeof(struct process) * nproc);
	
	for (int i = 0; i < nproc; i++) {
		scanf("%s%d%d", proc[i].name, &proc[i].t_ready, &proc[i].t_exec);
	}
	if (debug){
		printf("### INPUT ###\n");
		printf("%s\n", sched_policy);
		printf("%d\n", nproc);
		for (int i = 0; i < nproc; i++) {
			printf("%s %d %d\n", proc[i].name, proc[i].t_ready, proc[i].t_exec);
		}
		printf("----------------\n");
	}
//////////////////////////////////////////////////////////////////////////////////////////////////// POLICY str to int  AND  CHECK error	
	int policy;
	if 	(strcmp(sched_policy, "FIFO") 	== 0) {
		policy = FIFO;
	}
	else if (strcmp(sched_policy, "RR") 	== 0) {
		policy = RR;
	}
	else if (strcmp(sched_policy, "SJF") 	== 0) {
		policy = SJF;
	}
	else if (strcmp(sched_policy, "PSJF") 	== 0) {
		policy = PSJF;
	}
	else {
		printf("'%s' is not valid", sched_policy);
		exit(0);
	}
//////////////////////////////////////////////////////////////////////////////////////////////////// PREPARE EXECUTION
	// ソート by ready time
	qsort(proc, nproc, sizeof(struct process), cmp);
	// fork されてない状態に設定
	for (int i = 0; i < nproc; i++)
		proc[i].pid = -1;

	// 親プロセスの為にCPUを確保
	mask_cpu(getpid(), PARENT_CPU);
	prioritize(getpid());
	if (debug){
		printf("### PREPARE EXECUTION ###\n");
		printf("%s\n", sched_policy);
		printf("%d\n", nproc);
		for (int i = 0; i < nproc; i++) {
			printf("%s %d %d\n", proc[i].name, proc[i].t_ready, proc[i].t_exec);
		}
		printf("----------------\n");
	}

//////////////////////////////////////////////////////////////////////////////////////////////////// RECORED THE TIME OF EXECUTION
	// 現在の時間
	int current_time = 0;
	// 進行中のプロセスのインデックス
	int active_proc = -1;
	// 終了プロセス数
	int finish_cnt = 0;
	// プリエンプティブの時間
	int preemptive_rec = 0;
	// プリエンプティブの回数
	int preemptive_cnt = 0;
	// RRのときのコンテキストスイッチの時間
	int t_last = current_time;

	while(finish_cnt < nproc) {
		
		if (debug) {
			if (current_time%100==0) {
				printf("current_time: %d\n", current_time);
			}
		}
		// 終了確認
		if (active_proc != -1 && proc[active_proc].t_exec == 0) {
		
			if (debug) {
				printf("---  param status---\n");
				printf("| current_time      : %d  |\n", current_time);
				printf("| active_proc: %d  |\n",  active_proc);
				printf("| finish_cnt : %d  |\n", finish_cnt);
				printf("| t_last     : %d  |\n", t_last);
				printf("--------------------\n");
				printf("proc[active_proc].t_exec == 0 active_proc: %d\n", active_proc);
				printf("[waitpid]: %d\n", proc[active_proc].pid);
			}
			
			// https://manpages.debian.org/testing/manpages-ja-dev/waitpid.2.ja.html
			waitpid(proc[active_proc].pid, NULL, 0);
			printf("%s %d\n", proc[active_proc].name, proc[active_proc].pid);
			
			finish_cnt++;
			active_proc = -1;
		}

		// プロセス開始時間と終了時間の記録
		for (int i = 0; i < nproc; i++) {
			if (proc[i].t_ready == current_time) {
				proc[i].pid = execute(proc[i]);
				priority_idling(proc[i].pid);
				if (debug) {
					printf("proc[i].t_ready == current_time i: %d\n", i);
				}
			}

		}

		// 次のプロセス探し
		int next = -1;
		if (policy == FIFO){
			// アクティブがあるときは、次も同じタスク
			if (active_proc != -1){
				next = active_proc;
			}
			// アクティブがないときは、探す
			else {
				for(int i = 0; i < nproc; i++) {
					// フォークしてないやつ
					if (proc[i].pid == -1){
						continue;
					}
					// すでに終わってるやつ
					else if (proc[i].t_exec == 0){
						continue;
					}	
					// readytime 最小が次のタスク （順番にソートしたから
					else if(next == -1){
						next = i; 
					}
					// readytime 最小が次のタスク
					else if(proc[i].t_ready < proc[next].t_ready){
						next = i; 
					}	
				}	
			}	
		}
		else if (policy == RR) {
			// とりあえず次のタスクもアクティブと同じ
			next = active_proc;
			// もしアクティブがなければ
			if (active_proc == -1) {
				for (int i = 0; i < nproc; i++) {
					// すでにフォークできてて、まだ完了してないタスク
					if (proc[i].pid != -1 && proc[i].t_exec > 0){
						next = i;
						break;
					}
				}
			}
			// Roundの時間がきたら、
			else if ((current_time - t_last) % 100 == 0) {
				for (int i = 1; i < nproc +1; i++) {
					// 次のタスク
					next = (active_proc + i) % nproc;
					// next フォーク済み かつ まだ完了してなければ、そのタスクで決定
					if (proc[next].pid != -1 && proc[next].t_exec != 0) {
						break;
					}
				}
			}		
		}
		else if (policy == SJF) {
			// アクティブがあるときは、次も同じタスク
			if (active_proc != -1) {
				next = active_proc;
			}
			// アクティブがないときは、探す
			else {
				for (int i = 0; i < nproc; i++) {
					// フォークしてないタスクはスキップ
					if (proc[i].pid == -1) {
						continue;
					}
					// すでに完了してるタスクはスキップ
					else if (proc[i].t_exec == 0){
						continue;
					}
					// それ以外ならとりあえずnextにアサイン
					else if(next == -1){
						next = i; 
					}
					// もし、実行時間が短ければアサイン
					else if(proc[i].t_exec < proc[next].t_exec){
						next = i; 
					}
				}
			}
		}

		else if (policy == PSJF) {
			for (int i = 0; i < nproc; i++) {
				// フォークしてないタスクはスキップ
				if (proc[i].pid == -1) {
					continue;
				}
				// すでに完了してるタスクはスキップ
				else if (proc[i].t_exec == 0){
					continue;
				}
				// それ以外ならとりあえずnextにアサイン
				else if (next == -1){
					preemptive_rec = current_time;
					preemptive_cnt ++;
					next = i; 
				}
				// もし、実行時間が短ければアサイン
				else if (proc[i].t_exec < proc[next].t_exec){
					preemptive_rec = current_time;
					preemptive_cnt ++;
					next = i; 
				}
			}
		}
		else {
			printf("[SEARCHING NEXT PROCESS] '%d' is not valid", policy);
			exit(0);
		}
		
		// CONTEXT SWITCH
		// nextがアサインされてる
		if (next != -1) {
			// コンテクストスイッチする
			if (active_proc != next) {
				// プライオリティ上げ
				prioritize(proc[next].pid);
				// プライオリティ下げ
				priority_idling(proc[active_proc].pid);
				// コンテクストスイッチ
				active_proc = next;
				// RRのための記録
				t_last = current_time;
			}
		}

		// RUN PARENT PROCESS TIME
		volatile unsigned long i;		
		for (i = 0; i < 1000000UL; i++);
		// アクティブがあれば
		if (active_proc != -1)
			proc[active_proc].t_exec--;
		current_time++;
	}
	exit(0);
}
