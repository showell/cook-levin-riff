// test_stops.h: EXPECT_STOPS, a test that a call stops the program with a
// message, as a Python refusal raises. It comes first in a test file,
// before test.h and any system header, for _POSIX_C_SOURCE.

#ifndef COOK_LEVIN_TEST_STOPS_H
#define COOK_LEVIN_TEST_STOPS_H

#define _POSIX_C_SOURCE 200809L

#include "test.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

// The child's abort exits 134 rather than dump a core: the box's core
// handler takes a tenth of a second or more an abort.
static void aborted(int sig) {
    (void)sig;
    _exit(134);
}

// Runs `fn` in a child process; 1 when it stopped (exited nonzero or died
// of a signal) and its stderr holds `needle`. What it said is printed when
// not.
static int test_stops(void (*fn)(void), const char *needle) {
    int fds[2];
    if (pipe(fds) != 0) return 0;
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        signal(SIGABRT, aborted);
        close(fds[0]);
        dup2(fds[1], 2);
        fn();
        _exit(0);
    }
    close(fds[1]);
    char said[4096];
    size_t len = 0;
    ssize_t n;
    while ((n = read(fds[0], said + len, sizeof said - 1 - len)) > 0) len += (size_t)n;
    close(fds[0]);
    said[len] = 0;
    int status = 0;
    waitpid(pid, &status, 0);
    int stopped = WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    int ok = stopped && strstr(said, needle) != NULL;
    if (!ok) fprintf(stderr, "  wanted a stop saying \"%s\"; it said: %s\n", needle, said);
    return ok;
}

#define EXPECT_STOPS(fn, needle) EXPECT(test_stops(fn, needle))

#endif
