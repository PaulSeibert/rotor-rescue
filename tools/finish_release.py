#!/usr/bin/env python3
"""Finishes what `agk release` makes, for ROTOR RESCUE: run it after every
`agk release`.

  - Requirements: 1 MB (the kit's release tool always writes 512 KB)
  - the zip gets the demo GIF, dpslogo.bin.pk and titlelogo.bin.pk (the
    logos, for hard-disk installs), and its README.txt the copyright and the
    engine's licences

    agk release && python3 tools/finish_release.py
"""
import glob
import os
import shutil
import zipfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OLD_REQ = 'Any Amiga with 512 KB of chip RAM and Kickstart 1.3 or later (OCS, PAL).'
NEW_REQ = ('An Amiga 500 with 1 MB of RAM (512 KB chip + 512 KB expansion)\n'
           'and Kickstart 1.3 or later, PAL.')


def readme_txt(t):
    assert OLD_REQ in t, 'the requirements line has changed: update tools/finish_release.py'
    t = t.replace(OLD_REQ, NEW_REQ)
    t = t.replace('anywhere and run it from the Shell or Workbench.\n',
                  'anywhere, together with dpslogo.bin.pk (the Desert Palms Software logo) and\n'
                  'titlelogo.bin.pk (the title logo), and run it from its own drawer. Without\n'
                  'dpslogo.bin.pk it starts at the title; without titlelogo.bin.pk the title\n'
                  'shows no logo.\n'
                  'rotor-rescue-demo.gif is a 12-second clip of the game in action.\n')
    t = t.replace('By Paul Ross Seibert.\n',
                  'By Paul Ross Seibert.\n'
                  '(c) 2026 Desert Palms Software. All rights reserved.\n\n'
                  'Built with the ACE game engine (Mozilla Public License 2.0,\n'
                  'https://github.com/AmigaPorts/ACE), including its ProTracker player, and the\n'
                  'Amiga Game Kit runtime (MIT License).\n')
    return t


def main():
    rel = os.path.join(ROOT, 'build', 'release')
    (zpath,) = glob.glob(os.path.join(rel, '*.zip'))
    tmp = zpath + '.tmp'
    with zipfile.ZipFile(zpath) as a, zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as b:
        names = a.namelist()
        assert 'rotor-rescue-demo.gif' not in names, 'already finished: run agk release first'
        for i in a.infolist():
            d = a.read(i.filename)
            if i.filename == 'README.txt':
                d = readme_txt(d.decode()).encode()
            b.writestr(i, d)
        b.write(os.path.join(ROOT, 'demo', 'rotor-rescue.gif'), 'rotor-rescue-demo.gif')
        b.write(os.path.join(ROOT, 'build', 'disk', 'dpslogo.bin.pk'), 'dpslogo.bin.pk')
        b.write(os.path.join(ROOT, 'build', 'disk', 'titlelogo.bin.pk'), 'titlelogo.bin.pk')
    shutil.move(tmp, zpath)
    (rpath,) = glob.glob(os.path.join(rel, '*.readme'))
    t = open(rpath).read()
    assert OLD_REQ in t, 'the .readme requirements line has changed'
    open(rpath, 'w').write(t.replace(OLD_REQ, NEW_REQ))
    print(f'finished {os.path.basename(zpath)} and {os.path.basename(rpath)}')


if __name__ == '__main__':
    main()
