# ASH - A Unix-like Shell Implementation

A custom shell implementation in C that mimics the behavior of standard Unix shells like **bash** or **sh**.

> **⚠️ Note:** AI assistance (ChatGPT) was used for comment generation and explanation polishing, but all core logic and implementation were written manually.

## Project Layout (2025-06 Refactor)

```
ash/
├── src/               # all .c files
│   ├── shell.c        # entry point / main loop
│   ├── terminal.c     # tty + signal helpers
│   ├── jobs.c         # job-control API
│   ├── io.c           # I/O redirection helpers
│   ├── builtins.c     # cd, exit, export …
│   ├── history.c      # readline + history ring
│   ├── parser.c       # scripting language
│   ├── tokenizer.c    # lexer
│   ├── vars.c         # variable store
│   └── arith.c        # $(( … )) evaluator
├── include/           # public headers (added to include-path)
├── tests/             # lightweight unit tests (invoke `make test`)
├── Makefile           # pattern-rule build, `test` target
├── .clang-format      # Google-style, 2-space indent
└── .editorconfig      # editor-agnostic whitespace settings
```

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

- GCC (tested with ≥ 11)
- POSIX-compatible OS (Linux, *BSD, macOS)
- Development headers for `readline` (`sudo apt install libreadline-dev` on Debian/Ubuntu)
- GNU Make 4+

### Building the Shell

```bash
make        # builds src/*.c into ./ash
```

This will compile the code and create the executable `ash`.

### Running the Shell

Interactive shell:
```bash
./ash
```
Non-interactive script:
```bash
./ash script.ash arg1 arg2
```
One-off command:
```bash
./ash -c 'echo $((2+2))'
```

### Cleaning Up

```bash
# rebuild from scratch
make clean && make

# run unit tests
make test
```

### Coding Style & Tooling

• A project-wide **Google-style** `.clang-format` is provided; run `clang-format -i <file>`.
• `.editorconfig` keeps editors in sync (UTF-8, LF, 2-space indent, trim trailing whitespace).
• CI recommendation: `clang-format --dry-run --Werror $(SRC)`.

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

## Scripting Support

Ash now includes a **minimal scripting language** that re-uses the same parser in both
interactive and non-interactive modes.

### How to run a script

1. **Non-interactive** – execute an entire file and exit:

   ```bash
   ./ash myscript.ash
   ```

2. **Interactive** – from inside the REPL, source a file line-by-line:

   ```bash
   ash> source myscript.ash
   ```

### Currently supported scripting features

| Category | Syntax | Notes |
|----------|--------|-------|
| Variable assignment | `NAME=value` | No whitespace around `=`. Max 64 vars kept in memory. |
| Variable expansion  | `$NAME`      | Happens after tokenising, before exec. Undefined vars expand to empty string. |
| If statement        | `if <cmd>; then ... [else ...] fi` | Condition succeeds if `<cmd>` exits with status 0. |
| While loop          | `while <cmd>; do ... done` | Loop continues while condition command returns status 0. |
| For loop            | `for VAR in a b c; do ... done` | Iterates over list, setting `$VAR` each turn. |
| Export variable     | `export NAME[=value]` | Makes variable part of the environment for child processes. |
| Logical operators   | `cmd1 && cmd2`, `cmd1 \|\| cmd2` | Conditionally run second command based on first's exit status. |
| Case statement      | `case WORD in ... esac` | Shell glob patterns with `)` lines ending in `;;`. |
| Loop control        | `break`, `continue`     | Works inside `for` and `while` loops. |
| Source file         | `source <file>` | Executes the file in the current shell context. |
| Quoting / escapes   | Single quotes keep literal, double quotes allow \`\\\`, `\"`; backslash escapes outside quotes. |
| Functions           | `name() { ... }`       | Define shell functions and call them like commands. |

When running a script as `./ash script.ash arg1 arg2`, positional parameters `$1`, `$2` … are automatically set.

Support for `for … in … done` is planned for the next milestone.

### Example script `demo.ash`

```sh
# simple counter
count=0

while [ "$count" -lt 3 ]; do
    echo "Loop $count"
    count=$(expr $count + 1)
done

if ls *.c >/dev/null 2>&1; then
    echo "Found some C files!"
else
    echo "No C files here."
fi
```

Run it with `./ash demo.ash` or from the prompt with `source demo.ash`.

### One-off commands & quoting examples

```bash
# double-quoted string is preserved
./ash -c 'echo "hello world"'

# preserve spaces in variable value
./ash -c 'VAR="a b"; echo $VAR'

# logical AND / OR
./ash -c 'false && echo should_not_print'
./ash -c 'false || echo fallback'
```

## Running Unit Tests

Basic unit tests for the variable store and tokenizer live under `tests/`.

```bash
make test
```

You should see each individual test executable run and `All unit tests passed` when everything is green.

### Notes
• In the examples, text after the `#` symbol is a comment; do **not** paste the `# …` part into your terminal.

## Installation

```bash
# clone
git clone https://github.com/Albab-Hasan/Ash.git
cd ash

# build
make            # produces ./ash
make test       # optional: run unit tests
sudo make install  # copies ash to /usr/local/bin (optional target)
```

## Development Workflow

1. **Format** – run `clang-format -i $(git ls-files '*.c' '*.h')` before committing.
2. **Build**   – `make` (or `make debug` for ASan/UBSan, see Makefile).
3. **Test**    – `make test`.
4. **Run**     – `./ash`.

Continuous integration can use:
```bash
make && make test && clang-format --dry-run --Werror $(git ls-files '*.c' '*.h')
```

## Contributing

Pull requests are welcome!  Please:
1. Follow the coding style (clang-format config).
2. Add unit tests for new behaviour in `tests/`.
3. Keep functions small and modular—new subsystems get their own `src/*.c` and header.
4. Document new user-facing features in this README.

## License

This project is released under the MIT License (see `LICENSE`).

## Roadmap

- [ ] N-stage pipelines (currently limited to two commands)
- [ ] Command substitution `$(…)` & back-quotes
- [ ] Globbing / wild-cards expansion
- [ ] Heredoc (`<<EOF`)
- [ ] Built-in `alias` support
- [ ] Cross-platform test suite in GitHub Actions
