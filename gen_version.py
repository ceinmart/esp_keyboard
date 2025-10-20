import subprocess
from datetime import datetime
import os

# Get git info
def get_git(cmd):
    try:
        return subprocess.check_output(cmd, shell=True, encoding='utf-8').strip()
    except Exception:
        return 'unknown'

commit = get_git('git rev-parse --short HEAD')
branch = get_git('git rev-parse --abbrev-ref HEAD')
date = get_git('git log -1 --format=%cd')
path = get_git('git rev-parse --show-prefix')
if date == 'unknown':
    date = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

header = f"""
#ifndef VERSION_H
#define VERSION_H
#define GIT_COMMIT   \"{commit}\"
#define GIT_BRANCH   \"{branch}\"
#define GIT_DATE     \"{date}\"
#define GIT_PATH     \"{path}\"
#endif
"""

with open('version.h', 'w', encoding='utf-8') as f:
    f.write(header)
print('version.h gerado com sucesso.')
