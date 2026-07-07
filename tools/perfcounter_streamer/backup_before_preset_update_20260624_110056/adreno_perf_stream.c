// adreno_perf_stream.c
// Phone-side Adreno KGSL raw performance-counter streamer.
//
// Build for Android/arm64 with the NDK, push to /data/local/tmp, run through adb shell.
// The counter table is generated from a8xx_perfcntrs.xml into a8xx_perf_table.inc.

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define CSV_TEMP_PATH "/data/local/tmp/jerry_work/adreno_perf_stream_last.csv"

#define ADRENO_IOC_TYPE 0x09
#define MAX_SELECTED 64
#define MAX_MATCHES 32
#define DEFAULT_DEVICE "/dev/kgsl-3d0"

struct adreno_perfcounter_get {
  unsigned int group_id;
  unsigned int countable_selector;
  unsigned int regster_offset_low;
  unsigned int regster_offset_high;
  unsigned int __pad;
};
#define ADRENO_IOCTL_PERFCOUNTER_GET \
  _IOWR(ADRENO_IOC_TYPE, 0x38, struct adreno_perfcounter_get)

struct adreno_perfcounter_put {
  unsigned int group_id;
  unsigned int countable_selector;
  unsigned int __pad[2];
};
#define ADRENO_IOCTL_PERFCOUNTER_PUT \
  _IOW(ADRENO_IOC_TYPE, 0x39, struct adreno_perfcounter_put)

struct adreno_perfcounter_read_group {
  unsigned int group_id;
  unsigned int countable_selector;
  unsigned long long value;
};

struct adreno_perfcounter_read {
  struct adreno_perfcounter_read_group *groups;
  unsigned int num_groups;
  unsigned int __pad[2];
};
#define ADRENO_IOCTL_PERFCOUNTER_READ \
  _IOWR(ADRENO_IOC_TYPE, 0x3B, struct adreno_perfcounter_read)

struct counter_desc {
  const char *group_name;
  unsigned int group_id;
  unsigned int selector;
  const char *xml_name;
  const char *short_name;
};

#include "a8xx_perf_table.inc"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
  (void)sig;
  g_stop = 1;
}

static void sleep_seconds(double seconds) {
  if (seconds < 0.000001) seconds = 0.000001;
  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
  while (!g_stop && nanosleep(&req, &req) == -1 && errno == EINTR) {
  }
}

static void lower_squash(const char *in, char *out, size_t out_sz) {
  size_t j = 0;
  if (out_sz == 0) return;
  for (size_t i = 0; in[i] && j + 1 < out_sz; ++i) {
    unsigned char c = (unsigned char)in[i];
    if (isalnum(c)) out[j++] = (char)tolower(c);
  }
  out[j] = '\0';
}

static int contains_casefold(const char *haystack, const char *needle) {
  char h[256], n[256];
  lower_squash(haystack, h, sizeof(h));
  lower_squash(needle, n, sizeof(n));
  return n[0] && strstr(h, n) != NULL;
}

static int edit_distance_limited(const char *a, const char *b, int limit) {
  char aa[256], bb[256];
  lower_squash(a, aa, sizeof(aa));
  lower_squash(b, bb, sizeof(bb));
  int n = (int)strlen(aa), m = (int)strlen(bb);
  if (m >= 256 || abs(n - m) > limit) return limit + 1;
  int prev[256], cur[256];
  for (int j = 0; j <= m; ++j) prev[j] = j;
  for (int i = 1; i <= n; ++i) {
    cur[0] = i;
    int row_min = cur[0];
    for (int j = 1; j <= m; ++j) {
      int cost = aa[i - 1] == bb[j - 1] ? 0 : 1;
      int x = prev[j] + 1;
      int y = cur[j - 1] + 1;
      int z = prev[j - 1] + cost;
      int v = x < y ? x : y;
      if (z < v) v = z;
      cur[j] = v;
      if (v < row_min) row_min = v;
    }
    if (row_min > limit) return limit + 1;
    memcpy(prev, cur, (m + 1) * sizeof(int));
  }
  return prev[m];
}

static int score_counter(const struct counter_desc *c, const char *query) {
  if (contains_casefold(c->xml_name, query) || contains_casefold(c->short_name, query)) return 1000;
  if (contains_casefold(c->group_name, query)) return 500;

  char q[256], shortn[256], raw[256];
  lower_squash(query, q, sizeof(q));
  lower_squash(c->short_name, shortn, sizeof(shortn));
  lower_squash(c->xml_name, raw, sizeof(raw));
  if (!q[0]) return 0;

  int d1 = edit_distance_limited(q, shortn, 12);
  int d2 = edit_distance_limited(q, raw, 12);
  int d = d1 < d2 ? d1 : d2;
  int len = (int)strlen(q);
  if (d <= 3 || d * 3 <= len) return 300 - d;
  return 0;
}

struct match_item {
  size_t idx;
  int score;
};

static int cmp_match(const void *pa, const void *pb) {
  const struct match_item *a = (const struct match_item *)pa;
  const struct match_item *b = (const struct match_item *)pb;
  if (a->score != b->score) return b->score - a->score;
  const struct counter_desc *ca = &k_counters[a->idx];
  const struct counter_desc *cb = &k_counters[b->idx];
  int g = strcmp(ca->group_name, cb->group_name);
  if (g) return g;
  if (ca->selector < cb->selector) return -1;
  if (ca->selector > cb->selector) return 1;
  return 0;
}

static size_t find_matches(const char *query, struct match_item *out, size_t out_cap) {
  struct match_item *all = calloc(k_num_counters, sizeof(*all));
  if (!all) return 0;
  size_t n = 0;
  for (size_t i = 0; i < k_num_counters; ++i) {
    int s = score_counter(&k_counters[i], query);
    if (s > 0) {
      all[n].idx = i;
      all[n].score = s;
      ++n;
    }
  }
  qsort(all, n, sizeof(*all), cmp_match);
  size_t copy = n < out_cap ? n : out_cap;
  memcpy(out, all, copy * sizeof(*out));
  free(all);
  return copy;
}

static int already_selected(const size_t *selected, size_t n, size_t idx) {
  for (size_t i = 0; i < n; ++i) if (selected[i] == idx) return 1;
  return 0;
}

static void print_counter_line(size_t idx, int number) {
  const struct counter_desc *c = &k_counters[idx];
  if (number >= 0) printf("  [%2d] ", number);
  else printf("       ");
  printf("%-10s gid=0x%02x selector=%-4u %-44s (%s)\n",
         c->group_name, c->group_id, c->selector, c->short_name, c->xml_name);
}

static int prompt_select_from_matches(const char *query, size_t *selected, size_t *selected_n) {
  struct match_item matches[MAX_MATCHES];
  size_t n = find_matches(query, matches, MAX_MATCHES);
  if (n == 0) {
    fprintf(stderr, "[select] no counter match for '%s'\n", query);
    return -1;
  }

  printf("\n[select] matches for '%s':\n", query);
  for (size_t i = 0; i < n; ++i) print_counter_line(matches[i].idx, (int)i + 1);
  printf("Enter selection number(s), comma-separated; empty = first match; 0 = skip: ");
  fflush(stdout);

  char line[256];
  if (!fgets(line, sizeof(line), stdin)) return -1;
  if (line[0] == '\n' || line[0] == '\0') {
    if (*selected_n < MAX_SELECTED && !already_selected(selected, *selected_n, matches[0].idx)) {
      selected[(*selected_n)++] = matches[0].idx;
    }
    return 0;
  }

  char *save = NULL;
  for (char *tok = strtok_r(line, ", \t\r\n", &save); tok; tok = strtok_r(NULL, ", \t\r\n", &save)) {
    int choice = atoi(tok);
    if (choice == 0) continue;
    if (choice < 1 || (size_t)choice > n) {
      fprintf(stderr, "[select] ignoring invalid choice '%s'\n", tok);
      continue;
    }
    size_t idx = matches[choice - 1].idx;
    if (*selected_n < MAX_SELECTED && !already_selected(selected, *selected_n, idx)) {
      selected[(*selected_n)++] = idx;
    }
  }
  return 0;
}

static int add_query_or_prompt(const char *query, size_t *selected, size_t *selected_n, int interactive) {
  for (size_t i = 0; i < k_num_counters; ++i) {
    if (strcasecmp(query, k_counters[i].xml_name) == 0 || strcasecmp(query, k_counters[i].short_name) == 0) {
      if (*selected_n < MAX_SELECTED && !already_selected(selected, *selected_n, i)) selected[(*selected_n)++] = i;
      return 0;
    }
  }

  struct match_item matches[2];
  size_t n = find_matches(query, matches, 2);
  if (!interactive && n > 0) {
    if (*selected_n < MAX_SELECTED && !already_selected(selected, *selected_n, matches[0].idx)) selected[(*selected_n)++] = matches[0].idx;
    return 0;
  }
  return prompt_select_from_matches(query, selected, selected_n);
}

static int activate_counter(int fd, const struct counter_desc *c) {
  struct adreno_perfcounter_get p;
  memset(&p, 0, sizeof(p));
  p.group_id = c->group_id;
  p.countable_selector = c->selector;
  int ret = ioctl(fd, ADRENO_IOCTL_PERFCOUNTER_GET, &p);
  if (ret == -1) {
    fprintf(stderr, "[GET] failed: %-44s group=0x%x selector=%u: %s\n",
            c->short_name, c->group_id, c->selector, strerror(errno));
    return -1;
  }
  printf("[GET] %-44s group=0x%x selector=%u reg_low=0x%x reg_high=0x%x\n",
         c->short_name, c->group_id, c->selector, p.regster_offset_low, p.regster_offset_high);
  return 0;
}

static void deactivate_counter(int fd, const struct counter_desc *c) {
  struct adreno_perfcounter_put p;
  memset(&p, 0, sizeof(p));
  p.group_id = c->group_id;
  p.countable_selector = c->selector;
  if (ioctl(fd, ADRENO_IOCTL_PERFCOUNTER_PUT, &p) == -1) {
    fprintf(stderr, "[PUT] failed: %-44s group=0x%x selector=%u: %s\n",
            c->short_name, c->group_id, c->selector, strerror(errno));
  }
}

static int read_counters(int fd, unsigned int n, struct adreno_perfcounter_read_group *groups) {
  struct adreno_perfcounter_read p;
  memset(&p, 0, sizeof(p));
  p.groups = groups;
  p.num_groups = n;
  int ret = ioctl(fd, ADRENO_IOCTL_PERFCOUNTER_READ, &p);
  if (ret == -1) {
    fprintf(stderr, "[READ] failed: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

static double now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void print_usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s -i <seconds> <counter query> [counter query ...]\n", argv0);
  printf("  %s -l <counter search term>\n\n", argv0);
  printf("Counter query can be an exact counter name, a short name, or a fuzzy search term.\n");
  printf("Multiple counters can be selected in one command. Values printed on the terminal\n");
  printf("are deltas since the previous sample. elapsed_s is not printed on the terminal,\n");
  printf("but it is always saved in the CSV log.\n\n");
  printf("Options:\n");
  printf("  -i <seconds>       Sampling interval, for example 1 or 0.5. If omitted, prompts.\n");
  printf("  -d <device>        KGSL device path. Default: %s\n", DEFAULT_DEVICE);
  printf("  -n                 Non-interactive: choose best fuzzy match automatically.\n");
  printf("  -l <query>         List matching counters and exit. Use this to test fuzzy search.\n");
  printf("  --csv              Print CSV rows on terminal. Default terminal output is name=value.\n");
  printf("  -h                 Show this help.\n\n");
  printf("Examples:\n");
  printf("  %s -i 1 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES\n", argv0);
  printf("  %s -i 0.5 busy alu instruction\n", argv0);
  printf("  %s -n -i 0.2 busy alu\n", argv0);
  printf("  %s -l alu\n", argv0);
  printf("  %s -l fs_instruction\n", argv0);
}

static void cleanup_active(int fd, size_t active_n, const size_t *active_idx) {
  if (active_n == 0) return;
  printf("\n[cleanup] releasing %zu active counters\n", active_n);
  for (size_t i = 0; i < active_n; ++i) deactivate_counter(fd, &k_counters[active_idx[i]]);
}

static void csv_write_header(FILE *f, size_t active_n, const size_t *active_idx) {
  if (!f) return;
  fprintf(f, "elapsed_s");
  for (size_t i = 0; i < active_n; ++i) fprintf(f, ",%s", k_counters[active_idx[i]].short_name);
  fprintf(f, "\n");
  fflush(f);
}

static void csv_write_row(FILE *f, double elapsed_s, size_t active_n,
                          const size_t *active_idx,
                          const unsigned long long *diffv) {
  if (!f) return;
  fprintf(f, "%.6f", elapsed_s);
  for (size_t i = 0; i < active_n; ++i) {
    (void)active_idx;
    fprintf(f, ",%llu", diffv[i]);
  }
  fprintf(f, "\n");
  fflush(f);
}

static void strip_newline(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static int copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in) {
    fprintf(stderr, "[csv] failed to open temp CSV '%s': %s\n", src, strerror(errno));
    return -1;
  }
  FILE *out = fopen(dst, "wb");
  if (!out) {
    fprintf(stderr, "[csv] failed to open destination '%s': %s\n", dst, strerror(errno));
    fclose(in);
    return -1;
  }
  char buf[8192];
  size_t n;
  int ok = 0;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fprintf(stderr, "[csv] write failed for '%s': %s\n", dst, strerror(errno));
      ok = -1;
      break;
    }
  }
  if (ferror(in)) {
    fprintf(stderr, "[csv] read failed for '%s'\n", src);
    ok = -1;
  }
  fclose(in);
  if (fclose(out) != 0) {
    fprintf(stderr, "[csv] close failed for '%s': %s\n", dst, strerror(errno));
    ok = -1;
  }
  return ok;
}

static void prompt_save_csv(const char *tmp_path) {
  printf("\n[csv] samples were recorded to temporary file: %s\n", tmp_path);
  if (!isatty(STDIN_FILENO)) {
    printf("[csv] stdin is not interactive, so the file was kept at the temporary path.\n");
    return;
  }

  printf("Save CSV file? [Y/n]: ");
  fflush(stdout);
  char answer[32];
  if (!fgets(answer, sizeof(answer), stdin)) {
    printf("\n[csv] no answer received; keeping temporary CSV at %s\n", tmp_path);
    return;
  }
  strip_newline(answer);
  if (answer[0] == 'n' || answer[0] == 'N') {
    if (unlink(tmp_path) == 0) printf("[csv] discarded temporary CSV.\n");
    else fprintf(stderr, "[csv] failed to delete temporary CSV '%s': %s\n", tmp_path, strerror(errno));
    return;
  }

  printf("Destination path on the phone [default: %s]: ", tmp_path);
  fflush(stdout);
  char dst[512];
  if (!fgets(dst, sizeof(dst), stdin)) {
    printf("\n[csv] no destination received; keeping CSV at %s\n", tmp_path);
    return;
  }
  strip_newline(dst);
  if (dst[0] == '\0' || strcmp(dst, tmp_path) == 0) {
    printf("[csv] saved at %s\n", tmp_path);
    return;
  }
  if (copy_file(tmp_path, dst) == 0) printf("[csv] saved copy at %s\n", dst);
}


int main(int argc, char **argv) {
  const char *dev = DEFAULT_DEVICE;
  double interval = -1.0;
  int interactive = 1;
  int csv = 0;
  const char *list_query = NULL;

  int new_argc = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--csv") == 0) {
      csv = 1;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  argc = new_argc;

  int opt;
  while ((opt = getopt(argc, argv, "i:d:l:nh")) != -1) {
    switch (opt) {
      case 'i': interval = atof(optarg); break;
      case 'd': dev = optarg; break;
      case 'l': list_query = optarg; break;
      case 'n': interactive = 0; break;
      case 'h': print_usage(argv[0]); return 0;
      default: print_usage(argv[0]); return 2;
    }
  }

  if (list_query) {
    struct match_item matches[MAX_MATCHES];
    size_t n = find_matches(list_query, matches, MAX_MATCHES);
    printf("[list] top %zu matches for '%s'\n", n, list_query);
    for (size_t i = 0; i < n; ++i) print_counter_line(matches[i].idx, (int)i + 1);
    return n ? 0 : 1;
  }

  size_t selected[MAX_SELECTED];
  size_t selected_n = 0;

  if (optind >= argc) {
    print_usage(argv[0]);
    printf("\nEnter counter search terms, comma-separated\n");
    printf("Example input: busy, alu, fs_instruction\n");
    printf("> ");
    fflush(stdout);
    char line[512];
    if (!fgets(line, sizeof(line), stdin)) return 2;
    char *save = NULL;
    for (char *tok = strtok_r(line, ",\r\n", &save); tok; tok = strtok_r(NULL, ",\r\n", &save)) {
      while (*tok && isspace((unsigned char)*tok)) ++tok;
      if (*tok) add_query_or_prompt(tok, selected, &selected_n, interactive);
    }
  } else {
    for (int i = optind; i < argc; ++i) add_query_or_prompt(argv[i], selected, &selected_n, interactive);
  }

  if (selected_n == 0) {
    fprintf(stderr, "No counters selected. Use -l <query> to inspect names.\n");
    return 2;
  }

  if (interval <= 0.0) {
    printf("Sampling interval in seconds: ");
    fflush(stdout);
    char line[64];
    if (!fgets(line, sizeof(line), stdin)) return 2;
    interval = atof(line);
    if (interval <= 0.0) interval = 1.0;
  }

  printf("\n[selected] %zu counters, interval %.6g s\n", selected_n, interval);
  for (size_t i = 0; i < selected_n; ++i) print_counter_line(selected[i], -1);

  int fd = open(dev, O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "open(%s) failed: %s\n", dev, strerror(errno));
    return 1;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  size_t active_idx[MAX_SELECTED];
  size_t active_n = 0;
  for (size_t i = 0; i < selected_n; ++i) {
    if (activate_counter(fd, &k_counters[selected[i]]) == 0) active_idx[active_n++] = selected[i];
  }
  if (active_n == 0) {
    fprintf(stderr, "No counters could be activated.\n");
    close(fd);
    return 1;
  }

  struct adreno_perfcounter_read_group *groups = calloc(active_n, sizeof(*groups));
  unsigned long long *oldv = calloc(active_n, sizeof(*oldv));
  unsigned long long *newv = calloc(active_n, sizeof(*newv));
  unsigned long long *diffv = calloc(active_n, sizeof(*diffv));
  if (!groups || !oldv || !newv || !diffv) {
    fprintf(stderr, "allocation failed\n");
    cleanup_active(fd, active_n, active_idx);
    free(groups);
    free(oldv);
    free(newv);
    free(diffv);
    close(fd);
    return 1;
  }

  printf("[csv] temp path for this run: %s\n", CSV_TEMP_PATH);
  if (unlink(CSV_TEMP_PATH) == -1 && errno != ENOENT) {
    fprintf(stderr, "[csv] failed to remove old temporary CSV '%s': %s\n",
            CSV_TEMP_PATH, strerror(errno));
    fprintf(stderr, "[csv] refusing to continue because pulling this path later could return a stale CSV.\n");
    cleanup_active(fd, active_n, active_idx);
    free(groups);
    free(oldv);
    free(newv);
    free(diffv);
    close(fd);
    return 1;
  }

  FILE *csv_file = fopen(CSV_TEMP_PATH, "w");
  if (!csv_file) {
    fprintf(stderr, "[csv] failed to open temporary CSV '%s': %s\n", CSV_TEMP_PATH, strerror(errno));
    fprintf(stderr, "[csv] refusing to continue because pulling this path later could return a stale CSV.\n");
    cleanup_active(fd, active_n, active_idx);
    free(groups);
    free(oldv);
    free(newv);
    free(diffv);
    close(fd);
    return 1;
  }
  csv_write_header(csv_file, active_n, active_idx);
  for (size_t i = 0; i < active_n; ++i) {
    const struct counter_desc *c = &k_counters[active_idx[i]];
    groups[i].group_id = c->group_id;
    groups[i].countable_selector = c->selector;
  }

  if (read_counters(fd, (unsigned int)active_n, groups) == 0) {
    for (size_t i = 0; i < active_n; ++i) oldv[i] = groups[i].value;
  }

  printf("\n[stream] Press Ctrl+C to stop. Values are deltas since previous sample.\n");
  if (csv_file) printf("[csv] logging elapsed_s and counter deltas to %s\n", CSV_TEMP_PATH);
  if (csv) {
    printf("elapsed_s");
    for (size_t i = 0; i < active_n; ++i) printf(",%s", k_counters[active_idx[i]].short_name);
    printf("\n");
  }
  fflush(stdout);

  double t0 = now_seconds();
  while (!g_stop) {
    sleep_seconds(interval);
    if (g_stop) break;
    if (read_counters(fd, (unsigned int)active_n, groups) != 0) break;
    double t = now_seconds() - t0;

    if (csv) {
      printf("%.6f", t);
    }

    for (size_t i = 0; i < active_n; ++i) {
      newv[i] = groups[i].value;
      diffv[i] = newv[i] - oldv[i];  // unsigned wrap is intentional
      if (csv) {
        printf(",%llu", diffv[i]);
      } else {
        if (i > 0) printf(", ");
        printf("%s=%llu", k_counters[active_idx[i]].short_name, diffv[i]);
      }
      oldv[i] = newv[i];
    }
    csv_write_row(csv_file, t, active_n, active_idx, diffv);
    printf("\n");
    fflush(stdout);
  }

  cleanup_active(fd, active_n, active_idx);
  if (csv_file) {
    fclose(csv_file);
    prompt_save_csv(CSV_TEMP_PATH);
  }
  free(groups);
  free(oldv);
  free(newv);
  free(diffv);
  close(fd);
  return 0;
}
