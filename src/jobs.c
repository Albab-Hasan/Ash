#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

// Our job table
job_t jobs[MAX_JOBS];
int job_count = 0;

// Initialize the job system
void jobs_init(void) {
  // Clear all job slots
  for (int i = 0; i < MAX_JOBS; ++i) {
    jobs[i].pid = -1;
    jobs[i].pgid = -1;
    jobs[i].job_id = -1;
    jobs[i].running = 0;
    jobs[i].foreground = 0;
    jobs[i].notified = 0;
    jobs[i].command[0] = '\0';
  }
  job_count = 0;
}

// Find an empty slot in the job table
static job_t *alloc_job_slot(void) {
  for (int i = 0; i < MAX_JOBS; ++i) {
    if (jobs[i].pid == -1) return &jobs[i];
  }
  return NULL;
}

// Add a new job to our table
int add_job(pid_t pid, pid_t pgid, const char *command, int bg) {
  // Find a free slot
  job_t *slot = alloc_job_slot();
  if (!slot) {
    fprintf(stderr, "ash: too many jobs\n");
    return -1;
  }

  // Fill in the details
  int id = (slot - jobs) + 1;  // Job IDs start at 1
  slot->pid = pid;
  slot->pgid = pgid;
  slot->job_id = id;
  slot->running = 1;
  slot->foreground = !bg;
  slot->notified = 0;

  // Copy the command string
  strncpy(slot->command, command ? command : "", MAX_INPUT_SIZE - 1);
  slot->command[MAX_INPUT_SIZE - 1] = '\0';

  job_count++;
  return id;
}

// Remove a job from our table
void remove_job(int job_id) {
  // Sanity checks
  if (job_id <= 0 || job_id > MAX_JOBS) return;

  job_t *j = &jobs[job_id - 1];
  if (j->pid == -1)  // Already removed
    return;

  // Clear the slot
  j->pid = j->pgid = -1;
  j->job_id = -1;
  j->running = j->foreground = j->notified = 0;
  j->command[0] = '\0';
  job_count--;
}

// Find a job by its process ID
job_t *find_job_by_pid(pid_t pid) {
  for (int i = 0; i < MAX_JOBS; ++i) {
    if (jobs[i].pid == pid) return &jobs[i];
  }
  return NULL;
}

job_t *get_job_by_id(int id); /* forward declared later maybe */

// Print the job list for the 'jobs' command
void list_jobs(void) {
  for (int i = 0; i < MAX_JOBS; ++i) {
    if (jobs[i].pid == -1)  // Skip empty slots
      continue;

    const char *status = jobs[i].running ? "Running" : "Stopped";
    printf("[%d] %d %s\t%s\n", jobs[i].job_id, jobs[i].pid, status, jobs[i].command);
  }
}

// Check if any background jobs have finished
void check_background_jobs(void) {
  pid_t pid;
  int status;

  // Non-blocking check for any child process status changes
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    job_t *job = find_job_by_pid(pid);
    if (!job) continue;

    if (WIFSTOPPED(status)) {
      // Process was stopped (Ctrl+Z)
      job->running = 0;
      if (!job->notified) {
        printf("\n[%d] Stopped: %s\n", job->job_id, job->command);
        job->notified = 1;
      }
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
      // Process finished or was killed
      if (!job->notified) {
        printf("\n[%d] Done: %s\n", job->job_id, job->command);
        job->notified = 1;
      }
      remove_job(job->job_id);
    }
  }
}
