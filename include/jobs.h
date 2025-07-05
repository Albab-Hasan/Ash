#ifndef ASH_JOBS_H
#define ASH_JOBS_H

#include <sys/types.h>

// Maximum number of background jobs we can track
#define MAX_JOBS 32

#ifndef MAX_INPUT_SIZE
#define MAX_INPUT_SIZE 1024
#endif

// Job info structure
typedef struct job {
  pid_t pid;                     // Process ID
  pid_t pgid;                    // Process group ID
  int job_id;                    // Job number (starts at 1)
  char command[MAX_INPUT_SIZE];  // Command string
  int running;                   // Is it running or stopped?
  int foreground;                // Is it in foreground?
  int notified;                  // Have we told the user about status changes?
} job_t;

// Global job list
extern job_t jobs[MAX_JOBS];
extern int job_count;

// Set up the job control system
void jobs_init(void);

// Add a new job to the list
int add_job(pid_t pid, pid_t pgid, const char *command, int bg);

// Remove a job from the list
void remove_job(int job_id);

// Print all jobs (for the jobs command)
void list_jobs(void);

// Find a job by its process ID
job_t *find_job_by_pid(pid_t pid);

// Check if background jobs have finished
void check_background_jobs(void);

// Job control helpers (implemented in shell.c)
void continue_job(job_t *job, int foreground);
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);

#endif
