import shutil
from os.path import join, getcwd
from sys import argv
shutil.make_archive(f'Examples32.zip', 'zip', join(getcwd(), "bin", "Win32", "Release Examples"))
shutil.make_archive(f'Examples64.zip', 'zip', join(getcwd(), "bin", "x64", "Release Examples"))
