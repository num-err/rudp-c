#!/usr/bin/env python

for size in [10000, 100000, 1000000]:
    f = open('%dlines' % size, 'w')
    for i in xrange(size):
        f.write('This is line number %d\n' % i)
    f.close()
