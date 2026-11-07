#include <cstdio>
#include <cstdlib>
extern "C" void lab_counters(unsigned *a) {
    a[0] = a[1] = a[2] = 0;
}
extern "C" void lab_write(const void *p, unsigned n) {
    if (std::fwrite(p, 1, n, stdout) != n)
        std::abort();
}
extern "C" int lab_main();
int main() {
    return lab_main();
}
