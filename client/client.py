#!/usr/env python3

import liblo
import argparse
from time import sleep

deck = 0
xwaxhost = 'localhost'
xwaxport = 7770
rcvport = 7771


def print_status(*args):
    print('Received status message:')
    names = ['deck number', 'track path', 'artist', 'title', 'song length',
             'elapsed', 'pitch', 'control', 'monitor']
    for lbl, v in zip(names, args[1]):
        print('  {}: {}'.format(lbl, v))
    print('End status message')


def print_monitor(*args):
    print("Monitor:")
    print(args[1][0])

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('action',
                        choices=['load',
                                 'status',
                                 'recue',
                                 'disconnect',
                                 'reconnect',
                                 'monitor',
                                 'quit'],
                        help='What to do')
    parser.add_argument('player', choices=[1, 2], nargs='?', type=int,
                        help='Which player')
    parser.add_argument('filename', nargs='?', default=None, help='Which file')
    parser.add_argument('-H', '--host', default=xwaxhost,
                        help='Hostname at which xwax is running')
    parser.add_argument('-p', '--port', type=int, default=xwaxport,
                        help='Port at which xwax is listening')

    args = parser.parse_args()
    if args.action == 'load' and args.filename is None:
        parser.parse_args(['--help'])

    if args.action == 'load':
        osc_address = '/xwax/load_track'
        # FIXME get title and artist from file
        osc_args = [args.player-1, args.filename, 'Afu-Ra', 'Defeat (Redio)']
    elif args.action == 'status':
        osc_address = '/xwax/get_status'
        osc_args = [args.player-1]

        server = liblo.ServerThread(rcvport)
        server.add_method('/xwax/status', "isssfffi", print_status)
        server.start()
    elif args.action == 'monitor':
        osc_address = '/xwax/get_monitor'
        osc_args = [args.player-1]

        server = liblo.ServerThread(rcvport)
        server.add_method('/xwax/monitor', 's', print_monitor)
        server.start()
    elif args.action in ['recue', 'disconnect', 'reconnect']:
        osc_address = '/xwax/{}'.format(args.action)
        osc_args = [args.player-1]
    elif args.action == 'quit':
        osc_address = '/xwax/quit'
        osc_args = []
    else:
        raise ValueError('This should never happen')

    msg = liblo.Message(osc_address, *osc_args)
    addr = liblo.Address(args.host, args.port)       # This is where xwax is
    liblo.send(addr, msg)

    if args.action in ['status', 'monitor']:
        if args.host != 'localhost':
            print('Warning: xwax currently sends response to localhost, so UDP traffic may need to be redirected')
        sleep(.4)
        server.stop()
