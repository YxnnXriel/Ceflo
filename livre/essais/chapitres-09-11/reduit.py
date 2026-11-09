# reduit.py sortie.c OP_A OP_B ... : une copie de src/vm/vm.c qui ne garde que ces instructions (en switch).
import re, sys
src=open('src/vm/vm.c').read().split('\n')
keep=set(sys.argv[2:])
start=next(k for k,l in enumerate(src) if l.strip()=='// ======== valeurs ========')
end=next(k for k,l in enumerate(src) if l.startswith('#ifdef USE_COMPUTED_GOTO') and k>start)
out=src[:start]; block=[]; cur=None
def flush():
    global block
    if cur is None or cur in keep: out.extend(block)
    block=[]
for l in src[start:end]:
    m=re.match(r'    CASE\((OP_\w+)\)',l)
    if m: flush(); cur=m.group(1)
    elif re.match(r'    (divide_slow|modulo_slow):',l): flush(); cur='OP_DIVIDE' if 'divide' in l else 'OP_MODULO'
    elif l.startswith('    // ========'): flush(); cur=None
    block.append(l)
flush(); out+=src[end:]
s='\n'.join(out).replace('#if defined(__GNUC__) || defined(__clang__)\n    #define USE_COMPUTED_GOTO 1','#if 0\n    #define USE_COMPUTED_GOTO 1')
open(sys.argv[1],'w').write(s)
