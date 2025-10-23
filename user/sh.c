// Shell.
// TODO 
// when process file command, do not print $ prompt until command is done
// add support for wait
// add support for ';' to run multiple commands in one line
// add support for tab completion
// add support for history (up arrow to get last command)

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"

// Is the shell running interactively?
static int interactive = 0;

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

// A command is a sequence of possibly piped commands.
struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void debug_printcmd(struct cmd*, int);

// Execute cmd.  Never returns.
/// @brief Execute a command structure.
/// @param cmd The command structure to execute.
__attribute__((noreturn))
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

/// @brief Get a command from the user.
/// @param buf The buffer to store the command.
/// @param nbuf The size of the buffer.
/// @return 0 on success, -1 on EOF.
int
getcmd(char *buf, int nbuf)
{
  if (interactive)
    fprintf(2, "$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;
  struct stat st;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  if (fstat(0, &st) == 0 && (st.type == T_DEVICE)) {
    interactive = 1;
  } else {
    interactive = 0;
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    struct cmd* command = parsecmd(buf);
    // debug_printcmd(command, 0);
    if(fork1() == 0)
      runcmd(command);
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

/// @brief Create a new executable command.
/// @return A pointer to the new executable command structure.
struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

/// @brief Create a new redirection command.
/// @param subcmd The command to redirect.
/// @param file The file to redirect to/from.
/// @param efile The end pointer of the file string.
/// @param mode The file open mode.
/// @param fd The file descriptor to redirect.
/// @return A pointer to the new redirection command structure.
struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

/// @brief Create a new pipe command.
/// @param left The left command of the pipe.
/// @param right The right command of the pipe.
/// @return A pointer to the new pipe command structure.
struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

/// @brief Create a new list command.
/// @param left The left command of the list.
/// @param right The right command of the list.
/// @return A pointer to the new list command structure.
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

/// @brief Create a new background command.
/// @param subcmd The command to run in the background.
/// @return A pointer to the new background command structure.
struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

/// @brief  Get the next token from the input string.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @param q Pointer to the start of the token.
/// @param eq Pointer to the end of the token.
/// @return The token type.
int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

/// @brief Peek at the next token in the input string.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @param toks String of token characters to peek for.
/// @return Non-zero if the next token is in `toks`, zero otherwise.
int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

/// @brief Parse a command string into a command structure.
/// @param s The command string to parse.
/// @return A pointer to the parsed command structure.
struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

/// @brief Parse a line of input into a command structure.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @return A pointer to the parsed command structure.
struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  // Parse background commands
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  // Parse list commands
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

/// @brief Parse a pipeline of commands into a command structure.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @return A pointer to the parsed command structure.
struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

/// @brief Parse redirection operators in a command.
/// @param cmd The command to parse redirections for.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @return A pointer to the command structure with redirections parsed.
struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

/// @brief Parse a block of commands enclosed in parentheses.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @return A pointer to the parsed command structure.
struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

/// @brief Parse an executable command and its arguments.
/// @param ps Pointer to the current position in the input string.
/// @param es Pointer to the end of the input string.
/// @return A pointer to the parsed command structure.
struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

/// @brief NUL-terminate all the counted strings in a command structure.
/// @param cmd The command structure to process.
/// @return A pointer to the processed command structure.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}

// debug: print command tree
static void
print_indent(int d)
{
  while(d-- > 0) printf("  ");
}

/// @brief Debug function to print the command structure.
/// @param cmd The command structure to print.
/// @param depth The current indentation depth.
void
debug_printcmd(struct cmd *cmd, int depth)
{
  if(!cmd) { print_indent(depth); printf("NULL\n"); return; }
  switch(cmd->type){
  case EXEC: {
    struct execcmd *e = (struct execcmd*)cmd;
    print_indent(depth); printf("EXEC:");
    for(int i=0; e->argv[i]; i++){
      printf(" %s", e->argv[i]);
    }
    printf("\n");
    break;
  }
  case REDIR: {
    struct redircmd *r = (struct redircmd*)cmd;
    print_indent(depth); printf("REDIR fd=%d file=%s mode=%d\n", r->fd, r->file, r->mode);
    debug_printcmd(r->cmd, depth+1);
    break;
  }
  case PIPE: {
    struct pipecmd *p = (struct pipecmd*)cmd;
    print_indent(depth); printf("PIPE\n");
    debug_printcmd(p->left, depth+1);
    debug_printcmd(p->right, depth+1);
    break;
  }
  case LIST: {
    struct listcmd *l = (struct listcmd*)cmd;
    print_indent(depth); printf("LIST\n");
    debug_printcmd(l->left, depth+1);
    debug_printcmd(l->right, depth+1);
    break;
  }
  case BACK: {
    struct backcmd *b = (struct backcmd*)cmd;
    print_indent(depth); printf("BACK\n");
    debug_printcmd(b->cmd, depth+1);
    break;
  }
  default:
    print_indent(depth); printf("UNKNOWN type=%d\n", cmd->type);
  }
}
