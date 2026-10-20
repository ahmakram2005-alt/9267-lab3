/*
 *
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include <cerrno>
#include "command.h"
#include "tokenizer.h"

static int logFile = -1;

// Write one line for each finished child.
static void logChild()
{
    const char message[] = "Child terminated\n";

    if (logFile >= 0)
        write(logFile, message, sizeof(message) - 1);
}

// Collect finished background children.
static void reapChildren(int)
{
    int saved = errno;
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        logChild();

    errno = saved;
}

void parse(std::vector<Token> &tokens)
{
    /*
      Implement this function
      you must parse for all possible tokens scenario
      right now command.execute only print the format , you will inject it in your logic.

    */

    Command command;
    SimpleCommand *simple = new SimpleCommand();

    command.insertSimpleCommand(simple);

    bool valid = true;

    for (size_t i = 0; i < tokens.size(); i++)
    {
        Token token = tokens[i];

        if (token.type == TOKEN_EOF)
            break;

        // Add a word.
        if (token.type == TOKEN_COMMAND ||
            token.type == TOKEN_ARGUMENT)
        {
            simple->insertArgument(
                strdup(token.value.c_str())
            );
        }
        else if (token.type == TOKEN_PIPE)
        {
            if (simple->_numberOfArguments == 0)
            {
                valid = false;
                break;
            }

            // Start the next command.
            simple = new SimpleCommand();
            command.insertSimpleCommand(simple);
        }
        else if (token.type == TOKEN_BACKGROUND)
        {
            if (i + 1 >= tokens.size() ||
                tokens[i + 1].type != TOKEN_EOF)
            {
                valid = false;
                break;
            }

            command._background = 1;
        }
        else
        {
            // Read the filename after the operator.
            if (i + 1 >= tokens.size() ||
                (tokens[i + 1].type != TOKEN_ARGUMENT &&
                 tokens[i + 1].type != TOKEN_COMMAND))
            {
                valid = false;
                break;
            }

            std::string filename = tokens[++i].value;

            if (token.type == TOKEN_INPUT)
            {
                free(command._inputFile);
                command._inputFile = strdup(filename.c_str());
            }
            else if (token.type == TOKEN_ERROR)
            {
                free(command._errFile);
                command._errFile = strdup(filename.c_str());
                command._out_error = 0;
            }
            else
            {
                free(command._outFile);
                command._outFile = strdup(filename.c_str());

                command._append =
                    token.type != TOKEN_REDIRECT;

                if (token.type == TOKEN_REDIRECT_AND_ERROR)
                {
                    free(command._errFile);
                    command._errFile = strdup(filename.c_str());
                    command._out_error = 1;
                }
            }
        }
    }

    if (simple->_numberOfArguments == 0)
        valid = false;

    if (valid)
        command.execute();
    else
        fprintf(stderr, "Invalid command, pipe or redirection\n");

    command.clear();
    free(command._simpleCommands);
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;

    _arguments = (char **)malloc(
        _numberOfAvailableArguments * sizeof(char *)
    );

    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;

        _arguments = (char **)realloc(
            _arguments,
            _numberOfAvailableArguments * sizeof(char *)
        );
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;

    _simpleCommands = (SimpleCommand **)malloc(
        _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *)
    );

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;

        _simpleCommands = (SimpleCommand **)realloc(
            _simpleCommands,
            _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *)
        );
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        delete _simpleCommands[i];
    }

    if (_outFile)
        free(_outFile);

    if (_inputFile)
        free(_inputFile);

    if (_errFile)
        free(_errFile);

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);

        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }

        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");

    printf(
        "  %-12s %-12s %-12s %-12s %-12s\n",
        _outFile ? _outFile : "default",
        _inputFile ? _inputFile : "default",
        _errFile ? _errFile : "default",
        _out_error == 1 ? _errFile : "default",
        _background ? "YES" : "NO"
    );

    printf("\n\n");
    fflush(stdout);
}

void Command::execute()
{
    print();

    /*
        Write your code here
    */

    // Run cd and exit in the shell.
    char **args = _simpleCommands[0]->_arguments;

    if (_numberOfSimpleCommands == 1 &&
        (!strcmp(args[0], "cd") || !strcmp(args[0], "exit")))
    {
        if (_background || _inputFile || _outFile || _errFile)
        {
            fprintf(
                stderr,
                "Use cd and exit alone without redirection or &\n"
            );
            return;
        }

        if (!strcmp(args[0], "exit"))
        {
            printf("Good bye!!\n");
            exit(0);
        }

        if (_simpleCommands[0]->_numberOfArguments > 2)
        {
            fprintf(stderr, "cd: too many arguments\n");
            return;
        }

        const char *directory;

        if (args[1])
            directory = args[1];
        else
            directory = getenv("HOME");

        if (!directory)
            fprintf(stderr, "cd: HOME is not set\n");
        else if (chdir(directory) < 0)
            perror("cd");

        return;
    }

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        const char *name = _simpleCommands[i]->_arguments[0];

        if (!strcmp(name, "cd") || !strcmp(name, "exit"))
        {
            fprintf(stderr, "cd and exit cannot be used in pipes\n");
            return;
        }
    }

    int input = -1;
    int output = -1;
    int error = -1;

    // Open the input file.
    if (_inputFile)
    {
        input = open(_inputFile, O_RDONLY);

        if (input < 0)
        {
            perror(_inputFile);
            return;
        }
    }

    // Open the output file.
    if (_outFile)
    {
        int flags = O_WRONLY | O_CREAT;

        if (_append)
            flags |= O_APPEND;
        else
            flags |= O_TRUNC;

        output = open(_outFile, flags, 0666);

        if (output < 0)
        {
            perror(_outFile);

            if (input >= 0)
                close(input);

            return;
        }
    }

    // Open the error file.
    if (_errFile)
    {
        if (_out_error)
            error = dup(output);
        else
            error = open(
                _errFile,
                O_WRONLY | O_CREAT | O_TRUNC,
                0666
            );

        if (error < 0)
        {
            perror(_errFile);

            if (input >= 0)
                close(input);

            if (output >= 0)
                close(output);

            return;
        }
    }

    // Delay SIGCHLD while creating and waiting for children.
    sigset_t block, old;

    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old);

    std::vector<pid_t> children;
    int previous = input;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        int pipeFD[2] = {-1, -1};
        bool last = i == _numberOfSimpleCommands - 1;

        // Make a pipe for the next command.
        if (!last && pipe(pipeFD) < 0)
        {
            perror("pipe");
            break;
        }

        // Create one child.
        pid_t pid = fork();

        if (pid == 0)
        {
            if (_background)
                signal(SIGINT, SIG_IGN);
            else
                signal(SIGINT, SIG_DFL);

            signal(SIGCHLD, SIG_DFL);
            sigprocmask(SIG_SETMASK, &old, NULL);

            // Connect the input.
            if (previous >= 0 &&
                dup2(previous, STDIN_FILENO) < 0)
            {
                perror("dup2 input");
                _exit(1);
            }

            int destination;

            if (last)
                destination = output;
            else
                destination = pipeFD[1];

            // Connect the output.
            if (destination >= 0 &&
                dup2(destination, STDOUT_FILENO) < 0)
            {
                perror("dup2 output");
                _exit(1);
            }

            // Connect the errors.
            if (error >= 0 &&
                dup2(error, STDERR_FILENO) < 0)
            {
                perror("dup2 error");
                _exit(1);
            }

            // Close unused file descriptors.
            if (previous >= 0)
                close(previous);

            if (output >= 0)
                close(output);

            if (error >= 0)
                close(error);

            if (pipeFD[0] >= 0)
                close(pipeFD[0]);

            if (pipeFD[1] >= 0)
                close(pipeFD[1]);

            if (logFile >= 0)
                close(logFile);

            // Run the program.
            execvp(
                _simpleCommands[i]->_arguments[0],
                _simpleCommands[i]->_arguments
            );

            perror(_simpleCommands[i]->_arguments[0]);
            _exit(127);
        }

        if (pid < 0)
        {
            perror("fork");

            if (pipeFD[0] >= 0)
                close(pipeFD[0]);

            if (pipeFD[1] >= 0)
                close(pipeFD[1]);

            break;
        }

        children.push_back(pid);

        if (previous >= 0)
            close(previous);

        previous = pipeFD[0];

        if (pipeFD[1] >= 0)
            close(pipeFD[1]);
    }

    if (previous >= 0)
        close(previous);

    if (output >= 0)
        close(output);

    if (error >= 0)
        close(error);

    // Wait for foreground commands.
    if (!_background)
    {
        for (pid_t pid : children)
        {
            pid_t result;

            do {
                result = waitpid(pid, NULL, 0);
            } while (result < 0 && errno == EINTR);

            if (result > 0)
                logChild();
        }
    }
    else if (!children.empty())
    {
        printf(
            "Background PID: %ld\n",
            (long)children.back()
        );
    }

    sigprocmask(SIG_SETMASK, &old, NULL);
}

void Command::prompt()
{
    std::string input;

    while (true)
    {
        printf("myshell> ");
        fflush(stdout);

        // Read one line.
        if (!std::getline(std::cin, input))
        {
            printf("\nGood bye!!\n");
            break;
        }

        if (input.find_first_not_of(" \t\r") == std::string::npos)
            continue;

        std::vector<Token> tokens = tokenize(input);

        if (!tokens.empty())
            parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    // Open the log file.
    logFile = open(
        "process.log",
        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
        0666
    );

    if (logFile < 0)
    {
        perror("process.log");
        return 1;
    }

    // Keep the shell running after Ctrl+C.
    signal(SIGINT, SIG_IGN);

    // Handle finished background children.
    struct sigaction action = {};

    action.sa_handler = reapChildren;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &action, NULL);

    Command::_currentCommand.prompt();

    close(logFile);
    return 0;
}