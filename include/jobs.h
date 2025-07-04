#ifndef ASH_JOBS_H
#define ASH_JOBS_H

#include <sys/types.h>

#define MAX_JOBS 32
#ifndef MAX_INPUT_SIZE
#define MAX_INPUT_SIZE 1024
#endif

typedef struct job
{
  pid_t pid;                    // Process ID
  pid_t pgid;                   // Process group ID
  int job_id;                   // Job ID (1-based)
  char command[MAX_INPUT_SIZE]; // Command string
  int running;                  // 1 if running, 0 if stopped
  int foreground;               // 1 if foreground, 0 if background
  int notified;                 // 1 if status change already printed
} job_t;

extern job_t jobs[MAX_JOBS];
extern int job_count;

/* Lifecycle */
void jobs_init(void);

/* Management */
int add_job(pid_t pid, pid_t pgid, const char *command, int bg);
void remove_job(int job_id);
void list_jobs(void);
job_t *find_job_by_pid(pid_t pid);

/* Async reaping */
void check_background_jobs(void);

/* Control helpers implemented in shell.c (process-group / terminal) */
void continue_job(job_t *job, int foreground);
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);

#endif
