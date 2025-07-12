# ASH - My Unix-like Shell Project

This is a shell I've been building to learn more about how Unix shells work under the hood. It's inspired by bash and sh, but much simpler.

But now honestly calling this a learning project would be an understatement. I kinda amazed by how far I came building this. From trying to learn how to build a shell that supports basic commands like `cd` and `ls` to implementing globbing with wildcard support and Arbitrary Length Pipelines; with Job Control being hardest part. This has been a really fun journey and I'll continue building this project, hopefully one day making a 'Fish' equivelent.

Note: I know the commit history is all over the place, and I'm really sorry about that.

That said, things improve later on. Around commit #18 I cleaned up the project structure, and by commit #21 I started being more consistent with commits and writing proper messages.

Thanks for understanding.

## What's in the box

The shell supports basic features like:
- Running commands with arguments
- Built-ins like `cd`, `exit`, `history`, `alias` / `unalias`
- Background processes with `&`
- I/O redirection (`>`, `>>`, `<`)
- Pipes (`|`)
- Job control (Ctrl+Z, `jobs`, `fg`)
- Command history
- **Context-aware tab completion** (commands, files, variables)
- **Syntax highlighting in completion menu**
- Command substitution (`$(command)` and backtick syntax)
- **Wildcard / glob expansion** (`*.c`, `file?.txt`, `[abc]*`)
- **Command aliases** (`alias ll='ls -la'`)

## Project Structure

```
ash/
├── src/               # C source files
│   ├── shell.c        # Main shell code
│   ├── terminal.c     # Terminal handling
│   ├── jobs.c         # Background job management
│   ├── io.c           # Redirection stuff
│   ├── builtins.c     # Built-in commands
│   ├── history.c      # Command history
│   ├── parser.c       # Script parsing
│   ├── tokenizer.c    # Command tokenizing
│   ├── vars.c         # Environment variables
│   └── arith.c        # Math expressions
├── include/           # Header files
├── tests/             # Some basic tests
└── Makefile           # Build configuration
```

## Building & Running

### Requirements

- GCC
- readline library (`sudo apt install libreadline-dev` on Ubuntu/Debian)
- Make

### Building

Just run:
```bash
make
```

### Running

Interactive mode:
```bash
./ash
```

Run a script:
```bash
./ash script.ash arg1 arg2
```

Run a single command:
```bash
./ash -c 'echo hello world'
```

## Cool Features

### Job Control

The shell can handle background jobs properly:

```bash
# Start a job in background
ash> sleep 10 &
[1] 12345

# List running jobs
ash> jobs
[1] 12345 Running    sleep 10

# Bring to foreground
ash> fg 1
```

You can also suspend jobs with Ctrl+Z and resume them later with `fg` or `bg`.

### Pipes and Redirection

```bash
# Pipe output between commands
ash> ls | grep .txt

# Save output to file
ash> ls > files.txt

# Append to file
ash> echo "more stuff" >> files.txt

# Read from file
ash> sort < files.txt
```

### Command Substitution

You can use the output of a command as part of another command:

```bash
# Modern syntax
ash> echo "Today is $(date)"

# Traditional backtick syntax
ash> echo "Files: `ls | wc -l`"

# Nested substitutions
ash> echo "The date is $(echo `date`)"
```

### Pipelines and Redirection

```bash
# Multi-stage pipeline (any length)
ash> ls -1 | grep .c | sort | uniq -c | sort -nr
```

You can now chain as many commands as you like – every process in the pipeline shares the same process group so foreground signals (Ctrl-C / Ctrl-Z) work just like in bash.

### Scripting

I've added basic scripting support. Here's a simple example:

```sh
# Count to 3
count=0
while [ "$count" -lt 3 ]; do
    echo "Count: $count"
    count=$(expr $count + 1)
done

# Check for C files
if ls *.c >/dev/null 2>&1; then
    echo "Found some C files!"
else
    echo "No C files here."
fi
```

### Auto-Completion

- Tab-completion for commands, files, directories, and variables.
- Context-aware: first word completes commands, later words complete files/dirs, `$` triggers variable completion.
- Completion menu is colorized for easy visual parsing.

### Syntax Highlighting

- Completion menu highlights commands (green), operators (yellow), variables (cyan), strings (magenta), comments (gray), and arguments (white).
- Makes it easier to distinguish between different shell elements at a glance.

## Challenges I Faced

Building this shell taught me a lot about:

1. **Process management** - Creating, tracking, and controlling child processes
2. **Signal handling** - Dealing with Ctrl+C, Ctrl+Z, etc.
3. **Terminal control** - Managing which process group controls the terminal
4. **File descriptors** - Handling pipes and redirections properly

The trickiest part was getting job control working right - making sure background processes don't try to read from the terminal and that foreground processes receive signals properly.

## What's Next

I'm still working on:
- Heredocs (`<<EOF`)

## Building & Testing

```bash
# Regular build
make

# Run tests
make test

# Clean up
make clean
```

## License

This project is under the MIT License (see `LICENSE`).
