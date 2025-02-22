Import("env")

# General options that are passed to the C compiler (C only; not C++).
env.Append(CFLAGS=["-Wno-discarded-qualifiers", "-Wno-implicit-function-declaration"])

# General options that are passed to the C++ compiler
env.Append(CXXFLAGS=["-Wno-volatile"])