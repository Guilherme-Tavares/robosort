"""Traducao fiel do parser de 'mv re <r> es <e> [--arm]' do sketch."""
ZONE_COUNT = 5

def atoi(s):                      # C atoi: prefixo numerico, 0 se nao houver
    i, n = 0, 0
    while i < len(s) and s[i].isdigit():
        n = n*10 + int(s[i]); i += 1
    return n

def parse(cmd):
    if not cmd.startswith("mv re "):
        return ("nao casou", None, None, None)
    p = cmd[6:]
    p = p.lstrip(" ")
    region = atoi(p)
    p = p[len(p) - len(p.lstrip("0123456789")):]      # while isdigit p++
    p = p.lstrip(" ")
    if not p.startswith("es "):
        return ("uso", None, None, None)
    p = p[3:].lstrip(" ")
    state = atoi(p)
    p = p[len(p) - len(p.lstrip("0123456789")):]
    p = p.lstrip(" ")
    with_arm = (p == "--arm")
    if p and not with_arm:
        return ("uso", None, None, None)
    if region < 1 or region > ZONE_COUNT:
        return ("regiao", None, None, None)
    if state < 1 or state > 2:
        return ("estado", None, None, None)
    return (None, region, state, with_arm)

fails = 0
def ok(line, r, e, a):
    global fails
    got = parse(line)
    good = got == (None, r, e, a)
    print(f"{line:<26} -> {got}  {'' if good else '<<< FALHOU'}")
    fails += 0 if good else 1
def bad(line, why):
    global fails
    got = parse(line)
    good = got[0] == why
    print(f"{line:<26} -> {got[0]:<10} {'' if good else '<<< FALHOU'}")
    fails += 0 if good else 1

ok("mv re 1 es 1",       1, 1, False)
ok("mv re 4 es 2",       4, 2, False)
ok("mv re 5 es 2 --arm", 5, 2, True)
ok("mv re 1 es 1 --arm", 1, 1, True)
ok("mv re  3  es  2",    3, 2, False)
bad("mv re 6 es 1",   "regiao")
bad("mv re 0 es 1",   "regiao")
bad("mv re 1 es 3",   "estado")
bad("mv re 1 es 1 x", "uso")
bad("mv re 1",        "uso")
bad("mv re 1 es",     "uso")       # sem numero: nem chega a "es "
print("\n" + ("todos os casos passaram" if not fails else f"{fails} FALHA(S)"))
