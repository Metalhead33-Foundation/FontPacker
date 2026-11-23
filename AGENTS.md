**Never EVER put Makefiles, executables, libraries, .o files into any folder outside "build"! The "build" folder exists for a good reason! This is a QMake project!**

**Call `make` with an explicit absolute path.** In a terminal the shell rewrites `argv[0]`, so
   GNU make derives its `MAKE` variable from the Cursor binary and recursive `make` spawns new Cursor
   windows. Work around it with:

   ```bash
   env MAKE=/usr/bin/make /usr/bin/make -j16
   ```

   The same rewrite affects `QMAKE`, so a changed `.pro` file or a new subproject
   must be regenerated explicitly with `/usr/bin/qmake6`, and recursive make must
   inherit the override:

   ```bash
   env MAKE=/usr/bin/make /usr/bin/make -j16 QMAKE=/usr/bin/qmake6
   ```

**Call the compiler through `env` too.** The `argv[0]` rewrite hits the GCC driver as well: a bare
   `/usr/bin/g++ ...` typed into the Cursor terminal resolves its own program prefix (where `cc1plus`,
   `cc1`, `as`, `ld` live) from the rewritten `argv[0]`, so it looks next to the Cursor AppImage instead
   of `/usr/libexec/gcc/...` and dies with:

   ```
   Cursor-2.1.46-x86_64.AppImage: fatal error: cannot execute 'cc1plus': posix_spawnp: No such file or directory
   compilation terminated.
   ```

   The tell-tale sign is that the diagnostic is prefixed with `Cursor-…-x86_64.AppImage:` instead of
   `g++:` (the trailing errno text is localized, e.g. Hungarian "Nincs ilyen fájl vagy könyvtár"). It
   affects every GCC on the machine (`g++`, `g++-11`, `g++-16`, …), not just one version.
   `env` re-executes the real binary with a correct `argv[0]`, so prefix it on every *direct* compiler
   call:

   ```bash
   env /usr/bin/g++ -std=gnu++2a -I. -c foo.cpp -o /tmp/foo.o
   ```

   Equivalents, if `env` is not an option: `COMPILER_PATH=/usr/libexec/gcc/x86_64-pc-linux-gnu/15/` or
   `-B/usr/libexec/gcc/x86_64-pc-linux-gnu/15/` on the command line. The version directory must match
   the active driver (`/usr/bin/g++ -dumpversion`, `gcc-config -c` → `x86_64-pc-linux-gnu-15`).
   `GCC_EXEC_PREFIX` does **not** work for this. Compiler invocations spawned by `make` (the generated
   Makefiles use a bare `CXX = g++`) are unaffected — only the first process the terminal execs is
   rewritten — which is why the normal build works while a manual `g++` does not.

Whenever calling `git`, always use `--no-pager`.
