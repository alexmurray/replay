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
from gi.repository import Gtk
from gi.repository import Gio
from gi.repository import Replay

import json

VERSION = 1.0

class EventExporter(Replay.Task):
    __gtype_name__ = 'EventExporter'

    def __init__(self, path):
        Replay.Task.__init__(self, icon=Gtk.STOCK_SAVE, description='Exporting events to ' + path)
        self._path = path

    def event_to_dict(self, event):
        e = {}
        e['timestamp'] = event.get_timestamp()
        e['source'] = event.get_source()

        if isinstance(event, Replay.ActivityEndEvent):
            e['type'] =  'ReplayActivityEndEvent'
            e['id'] = event.get_id()
            e['node'] = event.get_node()
        elif isinstance(event, Replay.ActivityStartEvent):
            e['type'] =  'ReplayActivityStartEvent'
            e['id'] = event.get_id()
            e['node'] = event.get_node()
            e['props'] = event.get_props().unpack()
        elif isinstance(event, Replay.EdgeCreateEvent):
            e['type'] =  'ReplayEdgeCreateEvent'
            e['directed'] = event.get_directed()
            e['head'] = event.get_head()
            e['tail'] = event.get_tail()
            e['id'] = event.get_id()
            e['props'] = event.get_props().unpack()
        elif isinstance(event, Replay.EdgeDeleteEvent):
            e['type'] =  'ReplayEdgeDeleteEvent'
            e['id'] = event.get_id()
        elif isinstance(event, Replay.EdgePropsEvent):
            e['type'] =  'ReplayEdgePropsEvent'
            e['id'] = event.get_id()
            e['props'] = event.get_props().unpack()
        elif isinstance(event, Replay.MsgRecvEvent):
            e['type'] =  'ReplayMsgRecvEvent'
            e['id'] = event.get_id()
            e['node'] = event.get_node()
        elif isinstance(event, Replay.MsgSendEvent):
            e['type'] =  'ReplayMsgSendEvent'
            e['edge'] = event.get_edge()
            e['id'] = event.get_id()
            e['node'] = event.get_node()
            e['parent'] = event.get_parent()
            e['props'] = event.get_props().unpack()
        elif isinstance(event, Replay.NodeCreateEvent):
            e['type'] =  'ReplayNodeCreateEvent'
            e['id'] = event.get_id()
            e['props'] = event.get_props().unpack()
        elif isinstance(event, Replay.NodeDeleteEvent):
            e['type'] =  'ReplayNodeDeleteEvent'
            e['id'] = event.get_id()
        elif isinstance(event, Replay.NodePropsEvent):
            e['type'] =  'ReplayNodePropsEvent'
            e['id'] = event.get_id()
            e['props'] = event.get_props().unpack()
        else:
            self.emit_error('Unable to export unknown event: "%s"' % event)

        return e

    def export_event_store(self, event_store):
        dict_events = {'version': VERSION,
                       'events': [] }
        storeiter = event_store.get_iter_first()
        while storeiter != None:
            rpl_events = event_store.get_events(storeiter)
            for event in rpl_events:
                dict_event = self.event_to_dict(event)
                dict_events['events'].append(dict_event)
            storeiter = event_store.iter_next(storeiter)

        try:
            fp = open(self._path, 'w')
            json.dump(dict_events, fp)
            fp.close()
        except Exception as e:
            self.emit_error('%s' % e)
        finally:
            self.emit_progress(1.0)



UI_INFO = """
<ui>
  <menubar name='MenuBar'>
    <menu name='FileMenu' action='FileMenuAction'>
      <placeholder name='FileOps1'>
        <menuitem name='ExportEvents' action='ExportEventsAction'/>
      </placeholder>
    </menu>
  </menubar>
</ui>
"""

class ExportPlugin(GObject.Object, Replay.WindowActivatable):
    __gtype_name__ = 'ExportPlugin'

    window = GObject.property(type=Replay.Window)

    def on_export_action(self, widget):
        # create a dialog to choose the file to export to and then
        # create a EventExporter to do the actual export
        dialog = Gtk.FileChooserDialog("Export events...",
                                       self.window,
                                       Gtk.FileChooserAction.SAVE,
                                       (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                                        Gtk.STOCK_OK, Gtk.ResponseType.OK))
        dialog.set_do_overwrite_confirmation(True)

        filter = Gtk.FileFilter()
        filter.set_name('Replay Events')
        filter.add_pattern('*.rpl')
        dialog.add_filter(filter)
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            filename = dialog.get_filename()
            if not filename.endswith('.rpl'):
                filename = filename + '.rpl'
            exporter = EventExporter(filename)
            self.window.register_task(exporter)
            exporter.export_event_store(self.window.get_event_store())

        dialog.destroy()

    def do_activate(self):
        manager = self.window.get_ui_manager()
        self._action_group = Gtk.ActionGroup('ExportPluginActions')
        self._action_group.set_translation_domain('replay')
        self._action_group.add_actions([('ExportEventsAction',
                                        Gtk.STOCK_SAVE,
                                        'Export events',
                                        '<control>E',
                                        None,
                                        self.on_export_action)])
        manager.insert_action_group(self._action_group)

        self._ui_id = manager.add_ui_from_string(UI_INFO)
        manager.ensure_update()

        # when sensitivity of the global doc actions changes, make
        # sure we respect that
        def doc_actions_notify_sensitive_cb(doc_actions,
                                            pspec,
                                            data):
            self._action_group.set_sensitive(doc_actions.get_sensitive());

        doc_actions = self.window.get_doc_action_group();
        self._action_group.set_sensitive(doc_actions.get_sensitive());
        doc_actions.connect('notify::sensitive',
                            doc_actions_notify_sensitive_cb, self);


    def do_deactivate(self):
        manager = self.window.get_ui_manager()
        manager.remove_ui(self._ui_id)
        manager.remove_action_group(self._action_group)
        manager.ensure_update()
        self._action_group = None


class EventsFileLoader(Replay.FileLoader):
    __gtype_name__ = 'EventsFileLoader'

    def __init__(self):
        Replay.FileLoader.__init__(self, name='Replay Events', pattern='*.rpl')

    def props_to_variant(self, props):
        v = {}
        for name, value in props.iteritems():
            if type(value) == str or type(value) == unicode:
                v[name] = GLib.Variant('s', str(value))
            elif type(value) == int:
                v[name] = GLib.Variant('i', value)
            elif type(value) == float:
                v[name] = GLib.Variant('d', value)
            elif type(value) == bool:
                v[name] = GLib.Variant('b', value)
            else:
                self.emit_error('Unable to convert property of type "%s" to a GVariant - please report this as a bug at https://github.com/alexmurray/replay/issues' % str(type(value)))
        return GLib.Variant('a{sv}', v)

    def dict_to_event(self, e):
        timestamp = e['timestamp']
        source = e['source']

        if e['type'] == 'ReplayActivityEndEvent':
            event = Replay.ActivityEndEvent.new(timestamp,
                                                source,
                                                e['id'],
                                                e['node'])
        elif e['type'] == 'ReplayActivityStartEvent':
            event = Replay.ActivityStartEvent.new(timestamp,
                                                  source,
                                                  e['id'],
                                                  e['node'],
                                                  self.props_to_variant(e['props']))
        elif e['type'] == 'ReplayEdgeCreateEvent':
            event = Replay.EdgeCreateEvent.new(timestamp,
                                               source,
                                               e['id'],
                                               e['directed'],
                                               e['tail'],
                                               e['head'],
                                               self.props_to_variant(e['props']))
        elif e['type'] == 'ReplayEdgeDeleteEvent':
            event = Replay.EdgeDeleteEvent.new(timestamp,
                                               source,
                                               e['id'])
        elif e['type'] == 'ReplayEdgePropsEvent':
            event = Replay.EdgePropsEvent.new(timestamp,
                                              source,
                                              e['id'],
                                              self.props_to_variant(e['props']))
        elif e['type'] == 'ReplayMsgRecvEvent':
            event = Replay.MsgRecvEvent.new(timestamp,
                                            source,
                                            e['id'],
                                            e['node'])
        elif e['type'] == 'ReplayMsgSendEvent':
            event = Replay.MsgSendEvent.new(timestamp,
                                            source,
                                            e['id'],
                                            e['node'],
                                            e['edge'],
                                            e['parent'],
                                            self.props_to_variant(e['props']))
        elif e['type'] == 'ReplayNodeCreateEvent':
            event = Replay.NodeCreateEvent.new(timestamp,
                                               source,
                                               e['id'],
                                               self.props_to_variant(e['props']))
        elif e['type'] == 'ReplayNodeDeleteEvent':
            event = Replay.NodeDeleteEvent.new(timestamp,
                                               source,
                                               e['id'])
        elif e['type'] == 'ReplayNodePropsEvent':
            event = Replay.NodePropsEvent.new(timestamp,
                                              source,
                                              e['id'],
                                              self.props_to_variant(e['props']))
        else:
            self.emit_error('Unable to import unknown event: "%s"' % str(e))

        return event

    def do_load_file(self, file):
        path = file.get_path()
        fp = open(path, 'r')
        events_json = json.load(fp)
        if events_json['version'] != VERSION:
            self.emit_error('Invalid version "%f"' % events_json['version'])
            self.emit_progress(1.0)
            return

        for e in events_json['events']:
            event = self.dict_to_event(e)
            self.emit_event(event)
        self.emit_progress(1.0)


class ImportPlugin(GObject.Object, Replay.ApplicationActivatable):
    __gtype_name__ = 'ImportPlugin'

    application = GObject.property(type=Replay.Application)

    def do_activate(self):
        self._file_loader = EventsFileLoader()
        self.application.add_file_loader(self._file_loader)

    def do_deactivate(self):
        self.application.remove_file_loader(self._file_loader)
        self._file_loader = None
