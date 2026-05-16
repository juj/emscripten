import sys

print("stdout encoding:", sys.stdout.encoding)
print("stderr encoding:", sys.stderr.encoding)
sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')

print('A ä☃ö Z.cpp')
