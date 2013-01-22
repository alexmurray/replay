#
# Replay Visualisation System
# Copyright (C) 2012 Commonwealth of Australia
# Developed by Alex Murray <murray.alex@gmail.com>
#
# Replay is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 3 (GPLv3)
# (with extensions as allowed under clause 7 of the GPLv3) as
# published by the Free Software Foundation.
#
# Replay is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE
# file distributed with Replay for more details.
#

from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Replay

colors = ["#edd400",
          "#73d216",
          "#3465a4",
          "#75507b",
          "#f57900",
          "#c17d11",
          "#cc0000"]

class FDRFileLoader(Replay.FileLoader):
    __gtype_name__ = 'FDRFileLoader'

    def __init__(self):
        Replay.FileLoader.__init__(self, name='FDR Traces', pattern='*.fdr')

    def do_load_file(self, file):
        objects={}

        # just use standard python file loading as is easier
        path = file.get_path()
        f = open(path, 'r')
        data = f.read()
        statements = data.split(',')
        if len(statements) <= 1:
            self.emit_error('No FDR statements in %s' % path)
            return

        for statement in statements:
            tokens = statement.split('.')
            if len(tokens) != 4:
                self.emit_error('Invalid FDR statement "%s" - should consist of 4 tokens' % statement)
                return

            sender = tokens[0].strip()
            receiver = tokens[1].strip()
            op = tokens[2].strip()
            param = tokens[3].strip()

            if not sender in objects:
                objects[sender] = {'name': sender,
                                   'caps': []}
            if not receiver in objects:
                objects[receiver] = {'name': receiver,
                                     'caps': []}
            if not param in objects and param != 'null':
                objects[param] = {'name': param,
                                  'caps': []}
            if not receiver in objects[sender]['caps']:
                objects[sender]['caps'].append(receiver)
            if not param in objects[sender]['caps'] and param != 'null':
                objects[sender]['caps'].append(param)

        # emit node and edge create events - color as #999
        props = GLib.Variant('a{sv}', {'color': GLib.Variant('s', '#999'),})
        for obj in objects:
            event = Replay.NodeCreateEvent.new(timestamp=0, source=path, id=obj, props=props)
            self.emit_event(event)
        i = 0
        for obj in objects:
            for cap in objects[obj]['caps']:
                self.emit_event(Replay.EdgeCreateEvent.new(timestamp=0, source=path, id=str(i),
                                                        directed=True, tail=obj, head=cap, props=props))
                i = i + 1


        # now
        t = 0
        i = 0
        msg = []
        for statement in statements:
            tokens = map(lambda (s): s.strip(), statement.split('.'))
            if len(tokens) != 4:
                self.emit_error('Invalid FDR statement "%s" - should consist of 4 tokens' % statement)
                return

            sender = tokens[0]
            receiver = tokens[1]
            op = tokens[2]
            param = tokens[3]

            parent = None
            if len(msg) > 0:
                parent = str(msg[-1])
                self.emit_event(Replay.ActivityEndEvent.new(timestamp=t, source=path, id=parent,
                                                         node=sender))
            style = 'solid'
            if op =='Return':
                style = 'dashed'

            self.emit_event(Replay.MsgSendEvent.new(timestamp=t, source=path, id=str(i),
                                                 node=sender, edge=None,
                                                 parent=parent,
                                                 props=GLib.Variant('a{sv}',
                                                                    {'description': GLib.Variant('s', receiver + '(' + param + ')'),
                                                                     'line-style': GLib.Variant('s', style)})))
            t = t + 1
            self.emit_event(Replay.MsgRecvEvent.new(timestamp=t, source=path, id=str(i),
                                                 node=receiver))
            if op == 'Call':
                # add to call stack and start an activity for this call
                msg.append(i)
                self.emit_event(Replay.ActivityStartEvent.new(timestamp=t, source=path, id=str(i),
                                                           node=receiver,
                                                           props=GLib.Variant('a{sv}',
                                                                              {'description': GLib.Variant('s', receiver + '(' + param + ')'),

                                                                               'color': GLib.Variant('s', colors[i % len(colors)])})))
            else:
                del msg[-1]
                # resume parent activity if it exists
                if len(msg) > 0:
                    parent = str(msg[-1])
                    self.emit_event(Replay.ActivityStartEvent.new(timestamp=t, source=path, id=parent,
                                                               node=receiver,
                                                               props=GLib.Variant('a{sv}',
                                                                           {'description': GLib.Variant('s', receiver + '(' + param + ')'),
                                                                            'color': GLib.Variant('s', colors[int(parent) % len(colors)])})))

            i = i + 1
            t = t + 2

        # say we've finished
        self.emit_progress(1.0)

class FDRPlugin(GObject.Object, Replay.ApplicationActivatable):
    __gtype_name__ = 'FDRPlugin'

    application = GObject.property(type=Replay.Application)

    def do_activate(self):
        self._file_loader = FDRFileLoader()
        self.application.add_file_loader(self._file_loader)

    def do_deactivate(self):
        self.application.remove_file_loader(self._file_loader)
        self._file_loader = None
