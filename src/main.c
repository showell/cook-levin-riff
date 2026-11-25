// main.c: what virtual_machine.py does when run: test(), then
// find_solutions(), printing exactly what Python prints.
// A unity build: this file includes the C it needs, so it is one compile.

#include "vm.c"
#include "search.c"

int main(void) {
    vm_test();
    find_solutions(stdout);
    return fflush(stdout) == 0 ? 0 : 1;
}
