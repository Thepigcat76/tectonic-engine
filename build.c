/* GURD Build Tool - <https://github.com/Thepigcat76/gurd> */

#include "gurd.h"
#include <stdio.h>

#define COMPILER "clang"
#define STANDARD "c23"
#define DEBUG true
#define OUT_NAME "build/tectonic"

#define LIB_LILC "lilc"
#define LIB_RAYLIB "raylib"
#define LIB_GL "GL"
#define LIB_MATH "m"
#define LIB_DL "dl"
#define LIB_RT "rt"
#define LIB_PTHREAD "pthread"

static Cmd cmd = {0};

static void visit_entry(struct file_entry entry) {
  if (entry.file_ext == NULL || strcmp(entry.file_ext, "c") != 0)
    return;

  Cmd compile_cmd = {0};

  cmd_appendf(&compile_cmd, "ccache");
  cmd_appendf(&compile_cmd, COMPILER);

  // Compile source file in object mode
  cmd_appendf(&compile_cmd, "-c");
  cmd_appendf(&compile_cmd, "%s", entry.path);

  // Flags
  cmd_appendf(&compile_cmd, "-std=%s", STANDARD);
  if (DEBUG) {
    cmd_appendf(&compile_cmd, "-g");
    cmd_appendf(&compile_cmd, "-O1");
    cmd_appendf(&compile_cmd, "-fno-omit-frame-pointer");
    cmd_appendf(&compile_cmd, "-fsanitize=thread");
    cmd_appendf(&compile_cmd, "-pthread");
    cmd_appendf(&compile_cmd, "-D_GNU_SOURCE");
  }
  // Output location
  cmd_appendf(&compile_cmd, "-o");

  const char *src_path =
      strncmp(entry.path, "src/", 3) == 0 ? entry.path + 4 : entry.path;
  cmd_appendf(&compile_cmd, "./build/%s.o", src_path);

  // Ensure output location exists

  char build_path[512];
  sprintf(build_path, "./build/%s.o", src_path);
  ensure_parent_dirs(build_path, 0755);

  // Execute compile command

  if (cmd_execute(&compile_cmd)) {
    exit(1);
  }
}

static void visit_obj_entry(struct file_entry entry) {
  if (entry.file_ext == NULL || strcmp(entry.file_ext, "o") != 0)
    return;

  cmd_appendf(&cmd, "%s", entry.path);
}

static bool module_add(const char *module_directory) {
  bool success = gurd_build(module_directory, NULL);

  if (success) {
    char dir_buf[512];

    snprintf(dir_buf, 512, "%s/build", module_directory);

    printf("Collecting module %s\n", dir_buf);

    walk_dir(dir_buf, visit_obj_entry);
  }

  return success;
}

int main(int argc, char **argv) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "HOME is not set\n");
    return 1;
  }

  bool running = args_contains(argc, argv, "r") != -1;

  size_t backend_arg_idx =
      args_contains_len(argc, argv, "--backend=", strlen("--backend="));
  char *backend = NULL;
  if (backend_arg_idx != -1) {
    char *backend_arg = argv[backend_arg_idx];
    backend = backend_arg + strlen("--backend=");
  }

  // Remove old build files
  remove_dir_recursive("build", false);

  // Compile src files (cached)
  walk_dir("src", visit_entry);

  if (running) {
    walk_dir("example", visit_entry);
  }

  // Link obj files to binary
  cmd_appendf(&cmd, COMPILER);

  walk_dir("build", visit_obj_entry);

  // Libraries
  //cmd_appendf(&cmd, "-l" LIB_LILC);

  printf("Adding modules\n");

  // Modules
  module_add(str_fmt_temp("%s/coding/c/lilc", home));

  printf("Done adding modules\n");

  if (backend != NULL && strcmp(backend, "raylib") == 0) {
    cmd_appendf(&cmd, "-l%s", LIB_RAYLIB);
    cmd_appendf(&cmd, "-l%s", LIB_GL);
    cmd_appendf(&cmd, "-l%s", LIB_MATH);
    cmd_appendf(&cmd, "-l%s", LIB_DL);
    cmd_appendf(&cmd, "-l%s", LIB_RT);
    cmd_appendf(&cmd, "-l%s", LIB_PTHREAD);
  }

  cmd_appendf(&cmd, "-o");
  cmd_appendf(&cmd, OUT_NAME);
  cmd_appendf(&cmd, "-fsanitize=thread");
  cmd_appendf(&cmd, "-pthread");

  char s[1024];
  cmd_sprint(&cmd, s);

  // Run the command
  cmd_execute(&cmd);

  printf("Comamnd: %s\n", s);

  bool debug = args_contains(argc, argv, "-d") != -1;

  // Run program
  if (running) {
    if (debug) {
      systemf("gdb ./" OUT_NAME);
    } else if (args_contains(argc, argv, "-vg") != -1) {
      systemf("valgrind --leak-check=full --show-leak-kinds=all "
              "--track-origins=yes --errors-for-leak-kinds=all ./%s",
              OUT_NAME);
    } else {
      systemf("./" OUT_NAME);
    }
  }
}