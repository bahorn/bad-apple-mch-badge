import zlib
import sys
out = b''

with open(sys.argv[1], 'rb') as f:
    out = zlib.compress(f.read())

open(sys.argv[2], 'wb').write(out)
