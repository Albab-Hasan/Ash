#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

/* ------------------------------------------------------------------------- */
/*  Internal storage                                                          */
/* ------------------------------------------------------------------------- */

job_t jobs[MAX_JOBS];
int job_count = 0;

/* ------------------------------------------------------------------------- */
void jobs_init(void)
{
  for (int i = 0; i < MAX_JOBS; ++i)
  {
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

/* ------------------------------------------------------------------------- */
static job_t *alloc_job_slot(void)
{
  for (int i = 0; i < MAX_JOBS; ++i)
  {
    if (jobs[i].pid == -1)
      return &jobs[i];
  }
  return NULL;
}

/* ------------------------------------------------------------------------- */
int add_job(pid_t pid, pid_t pgid, const char *command, int bg)
{
  job_t *slot = alloc_job_slot();
  if (!slot)
  {
    fprintf(stderr, "ash: too many jobs\n");
    return -1;
  }

  int id = (slot - jobs) + 1; /* 1-based id */
  slot->pid = pid;
  slot->pgid = pgid;
  slot->job_id = id;
  slot->running = 1;
  slot->foreground = !bg;
  slot->notified = 0;
  strncpy(slot->command, command ? command : "", MAX_INPUT_SIZE - 1);
  slot->command[MAX_INPUT_SIZE - 1] = '\0';

  job_count++;
  return id;
}

/* ------------------------------------------------------------------------- */
void remove_job(int job_id)
{
  if (job_id <= 0 || job_id > MAX_JOBS)
    return;

  job_t *j = &jobs[job_id - 1];
  if (j->pid == -1)
    return;

  j->pid = j->pgid = -1;
  j->job_id = -1;
  j->running = j->foreground = j->notified = 0;
  j->command[0] = '\0';
  job_count--;
}

/* ------------------------------------------------------------------------- */
job_t *find_job_by_pid(pid_t pid)
{
  for (int i = 0; i < MAX_JOBS; ++i)
  {
    if (jobs[i].pid == pid)
      return &jobs[i];
  }
  return NULL;
}

job_t *get_job_by_id(int id); /* forward declared later maybe */

/* ------------------------------------------------------------------------- */
void list_jobs(void)
{
  for (int i = 0; i < MAX_JOBS; ++i)
  {
    if (jobs[i].pid == -1)
      continue;
    const char *status = jobs[i].running ? "Running" : "Stopped";
    printf("[%d] %d %s\t%s\n", jobs[i].job_id, jobs[i].pid, status, jobs[i].command);
  }
}

/* ------------------------------------------------------------------------- */
void check_background_jobs(void)
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
  {
    job_t *job = find_job_by_pid(pid);
    if (!job)
      continue;

    if (WIFSTOPPED(status))
    {
      job->running = 0;
      if (!job->notified)
      {
        printf("\n[%d] Stopped: %s\n", job->job_id, job->command);
        job->notified = 1;
      }
    }
    else if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      if (!job->notified)
      {
        printf("\n[%d] Done: %s\n", job->job_id, job->command);
        job->notified = 1;
      }
      remove_job(job->job_id);
    }
  }
}
