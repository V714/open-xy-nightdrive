extern "C" void *memset(void *p, int value, unsigned count) {
    auto q = static_cast<unsigned char *>(p);
    for (unsigned i = 0; i < count; ++i)
        q[i] = value;
    return p;
}
extern "C" void *memcpy(void *p, const void *s, unsigned count) {
    auto q = static_cast<unsigned char *>(p);
    auto t = static_cast<const unsigned char *>(s);
    for (unsigned i = 0; i < count; ++i)
        q[i] = t[i];
    return p;
}
