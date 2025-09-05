#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int
match(const char *regex, const char *str) {
  if (*regex == '\0' && *str == '\0')
    return 1;
  if (*regex == '*') {
    if (*(regex + 1) == '\0')
      return 1;
    for (int i = 0; *(str + i) != '\0'; i++) {
      if (match(regex + 1, str + i))
        return 1;
    }
    return 0;
  }
  if (*regex == '?' || *regex == *str) {
    return match(regex + 1, str + 1);
  }
  return 0;
}

void 
find(char *path, char *regex) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "find: %s is not a directory\n", path);
    close(fd);
    return;
  }

  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    printf("find: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;
    if (stat(buf, &st) < 0) {
      printf("find: cannot stat %s\n", buf);
      continue;
    }
    if (st.type == T_FILE) {
      if (match(regex, de.name)) {
        printf("%s\n", buf);
      }
    }
    else if (st.type == T_DIR) {
      find(buf, regex);
    }
  }
  close(fd);
  return;
}

int 
main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "Usage: find <path> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}