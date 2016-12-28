from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Replay
import json

class CausewayFileLoader(Replay.FileLoader):
    __gtype_name__ = 'CausewayFileLoader'

    def __init__(self):
        Replay.FileLoader.__init__(self, name='Causeway Traces', pattern='*.log')

    def validate_event(self, event):
        for (field, typ) in { 'class': list,
                              'anchor': dict,
                              'timestamp': int }.iteritems():
            if not field in event:
                raise Exception('Invalid Causeway log - missing required field "%s"' % field)
            if type(event[field]) != typ:
                raise Exception('Invalid Causeway log - required field "%s" must be of type "%s"' % (field, typ))
        return True

    def process_events(self, events):
        # order events based on their timestamp
        vats = {}
        t = None

        if type(events) != list:
            raise Exception('Invalid Causeway log - expected a list of events')
        # check each event contains the required fields with the
        # required types for each field
        for event in events:
            self.validate_event(event)
        # now process each event
        for event in sorted(events, lambda event: event['timestamp']):
            # get 0 timestamp
            if not t:
                t = event['timestamp']
            if ('org.ref_send.log.Sent' in event['class'] or
                'org.ref_send.log.Got' in event['class']):
                # the vat which sent this message - strip everything
                # preceeding the /-/ and strip any trailing /
                loop = event['anchor']['turn']['loop']
                name = loop.split('/-/')[1].strip('/')
                if not name in vats:
                    vats[name] = { 'name': name, 'objects': {} }
                vat = vats[name]
                # the object sending this message is the first calls
                # within trace - strip the method name though
                name = event['trace']['calls'][0]['name']
                name = '.'.join(name.split('.')[:-1])
                source = event['trace']['calls'][0]['source']
                if not name in vat['objects']:
                    vat['objects'][name] = { 'name': name, 'source': source }
                if 'org.ref_send.log.Sent' in event['class']:
                    sends.append(event)
                else:
                    recvs.append(event)

            elif 'org.ref_send.log.Resolved' in event['class']:
                # process promise resolution
                pass
            elif 'org.ref_send.log.Comment' in event['class']:
                # process comment
                pass
            else:
                raise Exception('Invalid Causeway log - Invalid event class "%s" -  must be one of Sent, Got or Comment' % str(event['class']))

        # create all objects in all vats
        for (vat_name, vat) in vats.iteritems():
            # TODO: color vats differently
            props = GLib.Variant('a{sv}', {'color': GLib.Variant('s', '#999'),
                                           'vat': GLib.Variant('s', vat_name)})
            for (obj_name, obj) in vat['objects'].iteritems():
                self.emit_event(Replay.NodeCreateEvent.new(timestamp=t, source=obj['source'], id=obj_name,
                                                        props=props))
        # send and recv all messages
        for event in sorted(events, lambda event: event['timestamp']):
            if 'org.ref_send.log.Sent' in event['class']:
                # message send event
                pass
            elif 'org.ref_send.log.Got' in event['class']:
                # message recv event
                pass
            elif 'org.ref_send.log.Comment' in event['class']:
                # message send event
                pass

    def do_load_file(self, file):
        # just use standard python file loading as is easier
        try:
            path = file.get_path()
            f = open(path, 'r')
            data = f.read()
            try:
                events = json.loads(data)
                try:
                    self.process_events(events)
                    # say we've finished successfully
                    self.emit_progress(1.0)
                except Exception as e:
                    print e
                    self.emit_error(str(e))
            except Exception as e:
                print e
                self.emit_error('Invalid JSON format: %s' % str(e))

        except Exception as e:
            self.emit_error('Error reading Causeway log %s: %s' % (path, str(e)))


class CausewayPlugin(GObject.Object, Replay.WindowActivatable):
    __gtype_name__ = 'CausewayPlugin'

    window = GObject.property(type=Replay.Window)

    def do_activate(self):
        self._file_loader = CausewayFileLoader()
        self.window.add_file_loader(self._file_loader)

    def do_deactivate(self):
        self.window.remove_file_loader(self._file_loader)
        self._file_loader = None
