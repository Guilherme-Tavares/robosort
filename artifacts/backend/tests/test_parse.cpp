// Testa o parser de 'mv re <r> es <e> [--arm]' no host, com o mesmo codigo
// do sketch. Nao toca em hardware: so extrai regiao, estado e o modificador.
#include <cstdio>
#include <cstring>
#include <cctype>

#define ZONE_COUNT 5

bool startsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

struct Result { const char* err; int region, state; bool withArm; };

Result parse(char* cmd) {
  Result r = {nullptr, -1, -1, false};
  if (!startsWith(cmd, "mv re ")) { r.err = "nao casou"; return r; }
  char* p = cmd + 6;
  while (*p == ' ') p++;
  int region = atoi(p);
  while (isdigit(*p)) p++;
  while (*p == ' ') p++;
  if (!startsWith(p, "es ")) { r.err = "uso"; return r; }
  p += 3;
  while (*p == ' ') p++;
  int state = atoi(p);
  while (isdigit(*p)) p++;
  while (*p == ' ') p++;
  bool withArm = !strcmp(p, "--arm");
  if (*p && !withArm) { r.err = "uso"; return r; }
  if (region < 1 || region > ZONE_COUNT) { r.err = "regiao"; return r; }
  if (state < 1 || state > 2)            { r.err = "estado"; return r; }
  r.region = region; r.state = state; r.withArm = withArm;
  return r;
}

int fails = 0;
void ok(const char* line, int region, int state, bool arm) {
  char buf[64]; strcpy(buf, line);
  Result r = parse(buf);
  bool good = !r.err && r.region == region && r.state == state && r.withArm == arm;
  printf("%-28s -> %s re=%d es=%d arm=%d %s\n", line,
         r.err ? r.err : "ok", r.region, r.state, r.withArm, good ? "" : "  <<< FALHOU");
  if (!good) fails++;
}
void bad(const char* line, const char* why) {
  char buf[64]; strcpy(buf, line);
  Result r = parse(buf);
  printf("%-28s -> %-8s %s\n", line, r.err ? r.err : "ACEITOU",
         (r.err && !strcmp(r.err, why)) ? "" : "  <<< FALHOU");
  if (!r.err || strcmp(r.err, why)) fails++;
}

int main() {
  ok("mv re 1 es 1",        1, 1, false);
  ok("mv re 4 es 2",        4, 2, false);
  ok("mv re 5 es 2 --arm",  5, 2, true);
  ok("mv re 1 es 1 --arm",  1, 1, true);
  ok("mv re  3  es  2",     3, 2, false);   // espacos extras
  bad("mv re 6 es 1",   "regiao");
  bad("mv re 0 es 1",   "regiao");
  bad("mv re 1 es 3",   "estado");
  bad("mv re 1 es 0",   "estado");
  bad("mv re 1",        "uso");
  bad("mv re 1 es 1 x", "uso");
  bad("mv re 1 es",     "estado");          // 'es' sem numero -> atoi 0
  printf(fails ? "\n%d FALHA(S)\n" : "\ntodos os casos passaram\n", fails);
  return fails != 0;
}
