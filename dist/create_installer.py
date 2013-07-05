#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
# Contact: http://engin.io
#
# You may use this file under the terms of the 3-clause BSD license.
# See the file LICENSE from this package for details.
#

from __future__ import print_function
import os
import shutil
import subprocess

# Qt
qmake = "qmake"
jom = "jom"

# from the installer framework
binarycreator = "binarycreator"

# visual studio
#setEnv = "c:\\Program Files\\Microsoft SDKs\\Windows\\v7.1\\Bin\\SetEnv.Cmd"


# Build Enginio

subprocess.check_call(["git", "clean", "-xdf"])

os.mkdir("build")
os.chdir("build")

# this is evil - keep the process alive so that we can set the right sdk env here
# and it may not work, at least currently I have conflicts with VS 7.1 and 8
#process = subprocess.Popen('cmd.exe /k\n', shell=True, stdin=subprocess.PIPE)
#process.stdin.write('call "' + setEnv + '"\n')
#process.stdin.write(qmake + " ../..\r\n")
#process.stdin.write(jom + "\n")

# docs build currently errors out!
# process.stdin.write("nmake docs\n")
#process.stdin.write("exit\n")
#process.communicate()

subprocess.check_call([qmake, "../.."])
print("Compiling Enginio...")

try:
    from subprocess import DEVNULL # py3k
except ImportError:
    DEVNULL = open(os.devnull, 'wb')

subprocess.check_call([jom,], stdout=DEVNULL)
subprocess.check_call(["nmake", "docs"])

# Copy files around
os.chdir("../..")

packages = {
    "com.digia.enginio": ["include", "lib", "qml", "mkspecs", "doc/enginio-qt.qch", ],
    "com.digia.enginioExamples": ["examples",],
    "com.digia.enginioDocumentation": ["doc/enginio-qt",],
    "com.digia.enginioSources": ["src",],
    }

print("Creating installer...")

for package in packages:
    for path in packages[package]:
        sourcePath = path
        if not os.path.exists(sourcePath):
            sourcePath = "dist/build/" + path
        if not os.path.exists(sourcePath):
            print("ERROR: Could not find path '" + path + "' in source or build directory.")
            exit(1)
        print("Creating ", path, " dir.")
        dest = "dist/packages/" + package + "/data/" + path
        if os.path.exists(dest):
            print("ERROR: ", dest, " exists already.")
            exit(1)
        if os.path.isfile(sourcePath):
            os.mkdir(os.path.dirname(dest))
            shutil.copyfile(sourcePath, dest)
        else:
            shutil.copytree(sourcePath, dest)

os.chdir("dist")
subprocess.check_call([binarycreator, "-c", "config\config.xml", "-p", "packages", "EnginioInstaller"])

print("Installer created.")
