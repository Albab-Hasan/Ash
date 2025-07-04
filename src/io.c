#include "io.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void handle_redirection(char **args, int *arg_count)
{
  int in_fd = -1, out_fd = -1;

  for (int i = 0; i < *arg_count; i++)
  {
    if (!args[i])
      break;

    /* Input redirection */
    if (strcmp(args[i], "<") == 0)
    {
      if (!args[i + 1])
      {
        fprintf(stderr, "ash: missing filename after <\n");
        exit(EXIT_FAILURE);
      }
      in_fd = open(args[i + 1], O_RDONLY);
      if (in_fd == -1)
      {
        perror("open");
        exit(EXIT_FAILURE);
      }
      if (dup2(in_fd, STDIN_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
      args[i] = NULL;
      *arg_count = i;
      break;
    }
    /* Output redirection overwrite */
    else if (strcmp(args[i], ">") == 0)
    {
      if (!args[i + 1])
      {
        fprintf(stderr, "ash: missing filename after >\n");
        exit(EXIT_FAILURE);
      }
      out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (out_fd == -1)
      {
        perror("open");
        exit(EXIT_FAILURE);
      }
      if (dup2(out_fd, STDOUT_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
      args[i] = NULL;
      *arg_count = i;
      break;
    }
    /* Output redirection append */
    else if (strcmp(args[i], ">>") == 0)
    {
      if (!args[i + 1])
      {
        fprintf(stderr, "ash: missing filename after >>\n");
        exit(EXIT_FAILURE);
      }
      out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (out_fd == -1)
      {
        perror("open");
        exit(EXIT_FAILURE);
      }
      if (dup2(out_fd, STDOUT_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
      args[i] = NULL;
      *arg_count = i;
      break;
    }
  }

  if (in_fd != -1)
    close(in_fd);
  if (out_fd != -1)
    close(out_fd);
}
