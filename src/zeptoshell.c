#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "zeptoshell.h"

int bg_pid[CMD_MAX] = {0};
int bg_count = 0;

bool sigint_caught = false;

struct cmd_s {
	int	argc;
	char	**argv;
	bool	bg;
	bool	piped;
};

int
builtin(char *argv[])
{
	const char *builtin_cmd[2];
	builtin_cmd[0] = "exit";
	builtin_cmd[1] = "quit";

	int cmd = -1;
	for (int i = 0; i < 2; i++)
	if (strcmp(builtin_cmd[i], argv[0]) == 0) {
		cmd = i;
		break;
	}

	switch (cmd) {
	case 0:
	case 1:
		exit(0);
	}
	
	return (cmd);
}

static bool
is_special(char c)
{
	switch (c) {
	case ';':
	case '&':
	case '|':
		return true;
	default:
		return false;
	}
}

void
init_sig()
{
	signal(SIGINT, sigint_handler);
	signal(SIGCHLD, sigchld_handler);
}

static void
print_bg()
{
	bool all_zero = true;
	for (int i = 0; i < CMD_MAX; i++) if (bg_pid[i]) {
		all_zero = false;
		if (kill(bg_pid[i], 0) == -1) {
			printf("[%d] done\n", i);
			bg_pid[i] = 0;
		}
	}
	
	if (all_zero) bg_count = 0;
}

static int
parse_line(const char *line, struct cmd_s *cmds)
{
	int count = 0, arg = 0, j = 0;
	int len = strlen(line);
	
	cmds[count].argv = malloc(sizeof(char*) * CMD_MAX);
	cmds[count].argv[arg] = malloc(CMD_MAX);

	if (len == 0) return (0);

	for (int i = 0; i < len; i++)
	switch (line[i]) {
	case ' ':
	case '\f':
	case '\r':
	case '\t':
	case '\v':
	  	while (isspace(line[i + 1])) i++;
		cmds[count].argv[arg][j] = '\0';
		if (!is_special(line[i + 1]))
			cmds[count].argv[++arg] = malloc(CMD_MAX);
		j = 0;
		break;
	case '\n':
	case ';':
next_cmd:
		if (i + 1 == len) break;
		if (is_special(line[i + 1])) break;
		cmds[count++].argc = arg + 1;
		arg = j = 0;
		cmds[count].argv = malloc(sizeof(char*) * CMD_MAX);
		cmds[count].argv[arg] = malloc(CMD_MAX);
		while (isspace(line[i + 1])) i++;
		break;
	case '|':
		cmds[count].piped = true;
		goto next_cmd;
	case '&':
		cmds[count].bg = true;
		goto next_cmd;
	default:
		cmds[count].argv[arg][j] = line[i];
		j++;
	}
	cmds[count].argc = arg + 1;
	return (count + 1);
}

void
prompt()
{
	putchar('>');
	putchar(' ');
	fflush(stdout);
}

bool
read_line(char *line)
{
	int i = 0;
	char c;
	bool comment = false;

	while (i < LINE_MAX) {
		c = getchar();
		switch (c) {
		case '#':
			comment = true;
			break;
		case EOF:
		case '\n':
			line[i] = '\0';
			return (true);
		default:
			if (!comment) line[i++] = c;
		}
	}

	fputs("Command line too long", stderr);
	return (false);
}

static void
run_cmds(struct cmd_s *cmds, int count)
{
	int i, p[2], fd_in = 0, status;
	pid_t pid;

	for (i = 0; i < count; i++) {
		if (sigint_caught) {
			sigint_caught = false;
			return;
		}
		if (builtin(cmds[i].argv) >= 0) continue;
		pipe(p);
		pid = fork();
		switch (pid) {
		case -1:
			perror("fork");
			exit(1);
		case 0:
			dup2(fd_in, 0);
			if (cmds[i].piped && i + 1 < count)
				dup2(p[1], 1);
			close(p[0]);
			execvp(cmds[i].argv[0], cmds[i].argv);
			perror(cmds[i].argv[0]);
			exit(errno);
		default:
			if (cmds[i].bg) {
				printf("[%d] %d\n", bg_count, pid);
				bg_pid[bg_count++] = pid;
			} else waitpid(pid, &status, 0);
			if (cmds[i].piped) fd_in = p[0];
			else fd_in = 0;
			close(p[1]);
		}
	}
}

void
run_line(char *line)
{
	struct cmd_s cmds[CMD_MAX];
	memset(&cmds, 0, sizeof(cmds));
	int count = parse_line(line, cmds);
	run_cmds(cmds, count);
}

static void
sigchld_handler(int signum)
{
	wait(NULL);
	print_bg();
}

static void
sigint_handler(int signum)
{
	sigint_caught = true;
	if (isatty(0)) {
		putchar('\n');
		prompt();
	}
}
