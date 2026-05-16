import sys

print("stdout encoding:", sys.stdout.encoding)
print("stderr encoding:", sys.stderr.encoding)

try:
  print('A ä☃ö Z.cpp')
except Exception as e:
  print(str(e))

sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')

print('A ä☃ö Z.cpp')
