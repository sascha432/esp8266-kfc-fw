# heap_analyze.py <serial log> - per request: heap at the access log line, low water mark of the render
import re, sys, collections

LOG = re.compile(r'^(\d+\.\d+) \S+ - - \[[^\]]*\] "(\w+) (\S+) HTTP/1\.\d" (\d+) (\S+) "([^"]*)" heap=(\d+)/(\d+) min=(\d+)')
POST = re.compile(r'^(\d+\.\d+) POST (\S+) body=(\d+) params=(\d+) heap=(\d+)/(\d+) min=(\d+)')
DONE = re.compile(r'^(\d+\.\d+) template done heap=(\d+)/(\d+) min=(\d+)')
WARN = re.compile(r'not enough memory to render the form, free heap (\d+) < (\d+)')
CRASH = re.compile(r'Exception \(|User exception|Soft WDT|wdt reset|last failed alloc')

rows = []
pending = None   # last access log line of a template page
post = None
for line in open(sys.argv[1], encoding='utf8', errors='replace'):
    m = POST.search(line)
    if m:
        post = dict(ts=float(m.group(1)), url=m.group(2), body=int(m.group(3)), params=int(m.group(4)),
                    heap=int(m.group(5)), block=int(m.group(6)), min=int(m.group(7)))
        continue
    m = LOG.search(line)
    if m:
        row = dict(ts=float(m.group(1)), method=m.group(2), url=m.group(3), code=int(m.group(4)),
                   ctype=m.group(6), heap=int(m.group(7)), block=int(m.group(8)), min=int(m.group(9)),
                   render_min=None, render_heap=None, post=post)
        post = None
        rows.append(row)
        pending = row if 'text/html' in row['ctype'] or row['code'] == 503 else None
        continue
    m = DONE.search(line)
    if m and pending:
        pending['render_heap'] = int(m.group(2)); pending['render_block'] = int(m.group(3)); pending['render_min'] = int(m.group(4))
        pending = None
        continue
    m = WARN.search(line)
    if m:
        rows.append(dict(method='WARN', url='guard', heap=int(m.group(1)), min=int(m.group(2)), code=503, post=post, render_min=None))
        continue
    if CRASH.search(line):
        rows.append(dict(method='CRASH', url=line.strip()[:100], heap=0, min=0, code=0, post=post, render_min=None))

print('%-6s %-44s %4s %7s %7s | %7s %7s %7s | %s' % ('meth', 'url', 'code', 'heap', 'min', 'rmin', 'rcost', 'rheap', 'post (heap at start / body / params / min while parsing)'))
worst = collections.defaultdict(lambda: [None, None])   # url -> (min heap after form creation, min render low water)
for r in rows:
    if r['method'] in ('WARN', 'CRASH'):
        print('%-6s %-44s %4s %7s %7s' % (r['method'], r['url'], r['code'], r['heap'], r['min']))
        continue
    p = r['post']
    ps = ('%u / %u / %u / %u' % (p['heap'], p['body'], p['params'], p['min'])) if p else ''
    rcost = (r['heap'] - r['render_min']) if r['render_min'] is not None else None
    print('%-6s %-44s %4u %7u %7u | %7s %7s %7s | %s' % (r['method'], r['url'][:44], r['code'], r['heap'], r['min'],
          r['render_min'] if r['render_min'] is not None else '-', rcost if rcost is not None else '-',
          r['render_heap'] if r['render_heap'] is not None else '-', ps))
    if r['render_min'] is not None and r['code'] == 200:
        w = worst[(r['method'], r['url'])]
        w[0] = r['heap'] if w[0] is None else min(w[0], r['heap'])
        w[1] = r['render_min'] if w[1] is None else min(w[1], r['render_min'])
        w.append(rcost)

print('\nworst case per page (code 200): heap after form creation, lowest render low water mark, max render cost')
for (m, u), w in sorted(worst.items(), key=lambda kv: kv[1][1]):
    print('%-6s %-44s heap=%6u rmin=%6u rcost_max=%6u' % (m, u[:44], w[0], w[1], max(w[2:])))
