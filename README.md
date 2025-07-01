# ASH - A Unix-like Shell Implementation

A custom shell implementation in C that mimics the behavior of standard Unix shells like bash or sh.

> **⚠️ Note:** AI assistance (Claude 3.7) was used for comment generation and explanation polishing, but all code logic and implementation were written manually.

## Features

- Command execution with argument support
- Built-in commands:
  - `cd [directory]` - Change directory
  - `exit` - Exit the shell
  - `history` - Show command history
  - `jobs` - List background jobs
  - `fg [job_id]` - Bring background job to foreground
- Background process execution using `&`
- Input/Output redirection:
  - `command > file` - Redirect output to file (overwrite)
  - `command >> file` - Redirect output to file (append)
  - `command < file` - Redirect input from file
- Pipeline support with `|` to connect multiple commands
- Job control with notifications for completed background jobs
- Signal handling for Ctrl+C (SIGINT) and Ctrl+Z (SIGTSTP)
- Command history navigation with arrow keys (using readline)
- Tab completion for commands and file paths
- Full terminal control with proper signal forwarding
- Advanced job control with foreground/background process group management

## Compilation and Execution

### Requirements

- GCC compiler
- Linux/Unix environment
- Readline development library (`libreadline-dev` on Debian/Ubuntu, `readline-devel` on CentOS/RHEL)

### Building the Shell

```bash
make
```

This will compile the code and create the executable `ash`.

### Running the Shell

```bash
./ash
```

### Cleaning Up

```bash
make clean
```

## Usage Examples

1. Execute a simple command:
```
ash> ls
```

2. Execute a command with arguments:
```
ash> ls -la
```

3. Change directory:
```
ash> cd /path/to/directory
```

4. Run a command in the background:
```
ash> sleep 10 &
```

5. Redirect output:
```
ash> ls > filelist.txt
```

6. Redirect input:
```
ash> sort < filelist.txt
```

7. Use a pipeline:
```
ash> ls | grep txt
```

8. View command history:
```
ash> history
```

9. View running jobs:
```
ash> jobs
```

10. Bring a background job to the foreground:
```
ash> fg 1
```

## Job Control Features

The shell implements true Unix job control with the following capabilities:

1. **Process Groups and Terminal Control**
   - Each pipeline of commands runs in its own process group
   - Uses `tcsetpgrp()` to give terminal control to the foreground process group
   - Background process groups don't have terminal access

2. **Job Management Commands**
   - `jobs` - List all active jobs with their state (running or stopped)
   - `fg <job_id>` - Bring a background job to the foreground
   - `bg <job_id>` - Continue a stopped job in the background
   - `&` (at end of command) - Run a command in the background

3. **Signal Handling**
   - Ctrl+C (SIGINT) and Ctrl+Z (SIGTSTP) are captured by the shell
   - Signals are properly forwarded to foreground process groups
   - Background jobs are notified when they complete

## Challenges and Solutions

1. **Process Control and Job Management**
   - Challenge: Keeping track of background jobs and their states.
   - Solution: Implemented a job array to store process IDs, process group IDs and statuses, with robust state tracking for completed background processes.

2. **Signal Handling**
   - Challenge: Ensuring the shell doesn't exit on Ctrl+C but properly handles signals for child processes.
   - Solution: Set up custom signal handlers for SIGINT and SIGTSTP to ignore them in the shell but pass them to foreground processes using process groups.

3. **Terminal Control**
   - Challenge: Implementing proper terminal access control for foreground and background jobs.
   - Solution: Used `tcsetpgrp()` to give terminal control to the foreground process group and restore it to the shell when the foreground job completes.

4. **Pipeline Implementation**
   - Challenge: Managing file descriptors and process creation for command pipelines.
   - Solution: Used the pipe() system call to create connected pipes between processes and properly redirect stdin/stdout, ensuring all processes in a pipeline share the same process group.

5. **Input/Output Redirection**
   - Challenge: Parsing and handling redirection operators with proper file permission management.
   - Solution: Detected redirection symbols during command parsing and used open(), dup2() to set up file descriptors before executing commands.
