# Lab 3 - Mini Shell

A simple shell written in C++.

## Build and Run

```bash
make
./myshell
```

## Features

- Run commands with arguments.
- Connect commands using pipes.
- Redirect input, output and errors.
- Run commands in the background using &.
- Use cd to change directory and exit to close the shell.
- Handle Ctrl+C and log child termination in process.log.

## Examples

```bash
ls -al
echo hello | grep hello
echo first > out.txt
echo second >> out.txt
cat < out.txt
ls /missing-folder 2> errors.txt
sleep 3 &
cd /tmp
exit
```

## Notes

- Use spaces around operators.
- Use cd and exit alone.
- Wildcards (* and ?) are not implemented.