/* GURD Build Tool - <https://github.com/Thepigcat76/gurd> */

#include "gurd.h"
#include <stdio.h>

#define COMPILER "clang"
#define STANDARD "c23"
#define DEBUG true

#define LIB_LILC "lilc"
#define LIB_CJSON "cjson"
#define LIB_RAYLIB "raylib"
#define LIB_GL "GL"
#define LIB_MATH "m"
#define LIB_DL "dl"
#define LIB_RT "rt"
#define LIB_PTHREAD "pthread"

static Cmd cmd = {0};
static bool builtin_server = false;

static void visit_entry(struct file_entry entry) {
  if (entry.file_ext == NULL || strcmp(entry.file_ext, "c") != 0)
    return;

  Cmd compile_cmd = {0};

  cmd_appendf(&compile_cmd, "ccache");
  cmd_appendf(&compile_cmd, COMPILER);

  // Compile source file in object mode
  cmd_appendf(&compile_cmd, "-c");
  cmd_appendf(&compile_cmd, "%s", entry.path);

  if (builtin_server)
    cmd_appendf(&compile_cmd, "-DBUILTIN_SERVER");

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

  builtin_server = args_contains(argc, argv, "--builtin-server") != -1;

  bool running = arg_eq(argc, argv, 1, "r") || arg_eq(argc, argv, 1, "run");
  bool building = arg_eq(argc, argv, 1, "b") || arg_eq(argc, argv, 1, "build");
  if (!running) building = true;

  if (argc <= 2 && running) {
    printf("[gurd-error] No run target provided, pass in the directory "
           "containing the entry point of your game, like: 'gurd run example "
           "...'\n");

    return 1;
  }
  const char *target = NULL;
  if (argc >= 3) {
    target = argv[2];
    if (running && strcmp(target, "engine") == 0) {
      printf("[gurd-error] cannot run 'engine'\n");
      return 1;
    }
  } else if (building) {
    target = "engine";
  }

  int backend_arg_idx =
      args_contains_len(argc, argv, "--backend=", strlen("--backend="));
  char *backend = NULL;
  if (backend_arg_idx != -1) {
    char *backend_arg = argv[backend_arg_idx];
    backend = backend_arg + strlen("--backend=");
  }

  // Remove old build files
  remove_dir_recursive("build", false);

  if (backend != NULL) {
    char path[256];
    sprintf(path, "backends/%s/", backend);

    walk_dir(path, visit_entry);
  } else {
    printf("[gurd-error]: No backend provided, specify one by passing "
           "--backend=... to gurd\n");
    return 1;
  }

  // Compile src files (cached)
  walk_dir("vendor", visit_entry);

  if (running) {
    if (target == NULL) {
      printf("[gurd-error] no target provided for running, specify a target "
             "after 'gurd run <example>'\n");
      return 1;
    }
    walk_dir("engine", visit_entry);
    walk_dir(target, visit_entry);
  } else if (building) {
    if (strcmp(target, "engine") != 0) {
      walk_dir("engine", visit_entry);
    }
    walk_dir(target, visit_entry);
  }

  // Link obj files to binary
  cmd_appendf(&cmd, COMPILER);

  walk_dir("build", visit_obj_entry);

  // Libraries
  cmd_appendf(&cmd, "-l" LIB_LILC);
  cmd_appendf(&cmd, "-l" LIB_CJSON);

  if (DEBUG) {
    cmd_appendf(&cmd, "-rdynamic");
  }

  if (backend != NULL && strcmp(backend, "raylib") == 0) {
    cmd_appendf(&cmd, "-l%s", LIB_RAYLIB);
    cmd_appendf(&cmd, "-l%s", LIB_GL);
    cmd_appendf(&cmd, "-l%s", LIB_MATH);
    cmd_appendf(&cmd, "-l%s", LIB_DL);
    cmd_appendf(&cmd, "-l%s", LIB_RT);
    cmd_appendf(&cmd, "-l%s", LIB_PTHREAD);
  }

  const char *out_name = strcmp(target, "engine") == 0 ? "tectonic" : target;

  cmd_appendf(&cmd, "-o");
  cmd_appendf(&cmd, "build/%s-bin", out_name);
  cmd_appendf(&cmd, "-fsanitize=thread");
  cmd_appendf(&cmd, "-pthread");

  // Run the command
  cmd_execute(&cmd);

  bool debug = args_contains(argc, argv, "-d") != -1;

  int args_arg_idx = args_contains(argc, argv, "--args");

  // Run program
  if (running) {
    char args[1024] = {'\0'};

    if (args_arg_idx != -1 && args_arg_idx + 1 < argc) {
      for (int i = args_arg_idx + 1; i < argc; i++) {
        strcat(args, argv[i]);
        if (i + 1 < argc) {
          strcat(args, " ");
        }
      }
    }

    char exec_cmd[1024];
    sprintf(exec_cmd, "./build/%s-bin", out_name);

    if (debug) {
      sprintf(exec_cmd, "gdb --args ./build/%s-bin", out_name);
    } else if (args_contains(argc, argv, "-vg") != -1) {
      sprintf(exec_cmd,
              "valgrind --leak-check=full --show-leak-kinds=all "
              "--track-origins=yes --errors-for-leak-kinds=all ./build/%s-bin",
              out_name);
    }

    return WEXITSTATUS(systemf("%s %s", exec_cmd, args));
  }
}