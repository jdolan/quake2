#!/usr/bin/python

from os import path, system
import sys

class Dictionary(dict):
    """A dict allowing dot notation."""
    def __getattr__(self, attr):
        return self.get(attr, None)
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__

config = Dictionary({
    'root': 'quake2world.net::/quake2',
    'base': [
        'baseq2/pak[1-3].pak',
        'baseq2/players/*',
        '3.20_changes.txt',
        'license.txt',
        'readme.txt',
    ],
    'ctf': [
        'ctf/*'
    ],
    'demo': [
        'baseq2/pak-quake2-demo.*'
    ],
    'hud': [
        'baseq2/pak-kmquake2-hud.*',
    ],
    'models': [
        'baseq2/pak-generations-models.*'
    ],
    'textures': [
        'baseq2/pak-jimw-textures.*'
    ]
})

messages = Dictionary({
    'begin': 'This script will install optional game data. Continue?',
    'home': 'Enter your Quake2 directory:',
    'base': 'Install the 3.20 point release?',
    'ctf': 'Install ThreeWave Capture the Flag?',
    'demo': 'Install the demo? Select this if you do not own Quake2. (48MB)',
    'hud': 'Install the KMQuake2 high-resolution HUD (0.5MB)?',
    'models': 'Install the Generations Arena high-resolution models?',
    'textures': 'Install JimW\'s high-resolution textures (320MB)?',
    'end': 'Installation complete.'
})

def _rsync(source, target):
    """Fetches a single source pattern to the specified target."""
    system("echo rsync -rzhP %s/%s %s" % (config.root, source, target))
    
def rsync(module):
    """Fetches all path patterns in the specified module."""
    for path in module:
        _rsync(path, config.home)

def prompt(message, default=''):
    """Prompts the user for input, returning their response."""
    while True:
        sys.stdout.write("%s [%s] " % (message, default))
        response = raw_input().lower()
        if response == '':
            if default == '':
                continue
            response = default
        return response.lower()

if __name__ == '__main__':
        
    if prompt(messages.begin, 'y') != 'y':
        sys.exit()
    
    config.home = path.expanduser('~/.quake2')
    config.home = prompt(messages.home, config.home)
        
    if prompt(messages.base, 'y') == 'y':
        rsync(config.base)
        
    if prompt(messages.ctf, 'y') == 'y':
        rsync(config.ctf)
        
    if prompt(messages.demo, 'n') == 'y':
        rsync(config.demo)
        
    if prompt(messages.hud, 'n') == 'y':
        rsync(config.hud)
    
    if prompt(messages.models, 'n') == 'y':
        rsync(config.models)
        
    if prompt(messages.textures, 'n') == 'y':
        rsync(config.textures)
        
    print messages.end
