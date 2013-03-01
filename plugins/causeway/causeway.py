from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Replay
import json

class Vat:
    index = 0
    # some tango colors to look pretty
    colors = [ '#edd400',
               '#f57900',
               '#73d216',
               '#3465a4',
               '#75507b',
               '#cc0000' ]


    @classmethod
    def next_color(cls):
        color = cls.colors[cls.index]
        cls.index = (cls.index + 1) % len(cls.colors)
        return color

    def __init__(self, name):
        self.name = name
        self.color = self.next_color()
        self.objects = {}
        self.iter_index = 0

    def add(self, obj):
        self.objects[obj.name] = obj

    def get(self, name):
        return self.objects[name]

    def contains(self, name):
        return name in self.objects

    def __iter__(self):
        return iter(self.objects)

class Obj:
    def __init__(self, name, source):
       self.name = name
       self.source = source

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
        for event in sorted(events, key=lambda event: event['timestamp']):
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
                    vats[name] = Vat(name)
                vat = vats[name]
                # the object sending this message is the first calls
                # within trace - strip the method name though
                if 'org.ref_send.log.Sent' in event['class']:
                    if not 'trace' in event:
                        print 'No trace in event ' + str(event)
                        continue
                    call = event['trace']['calls'][0]
                    name = 'name' in call and call['name'] or 'No name'
                    name = '.'.join(name.split('.')[:-1])
                    source = 'source' in call and call['source'] or 'No source'
                    if not vat.contains(name):
                        vat.add(Obj(name, source))

                else:
                    # TODO: do something sensible with got messages
                    print 'Unused event ' + str(event)
                    pass

            elif 'org.ref_send.log.Resolved' in event['class']:
                # process promise resolution
                print 'Unused event ' + str(event)
                pass
            elif 'org.ref_send.log.Comment' in event['class']:
                # process comment
                print 'Unused event ' + str(event)
                pass
            else:
                raise Exception('Invalid Causeway log - Invalid event class "%s" -  must be one of Sent, Got or Comment' % str(event['class']))

        # create all objects in all vats
        for (vat_name, vat) in vats.iteritems():
            props = GLib.Variant('a{sv}', {'color': GLib.Variant('s', vat.color),
                                           'vat': GLib.Variant('s', vat.name)})
            for obj_name in iter(vat):
                obj = vat.get(obj_name)
                self.emit_event(Replay.NodeCreateEvent.new(timestamp=t,
                                                           source=obj.source,
                                                           id=obj.name,
                                                           props=props))
        # send and recv all messages
        for event in sorted(events, key=lambda event: event['timestamp']):
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


class CausewayPlugin(GObject.Object, Replay.ApplicationActivatable):
    __gtype_name__ = 'CausewayPlugin'

    application = GObject.property(type=Replay.Application)

    def do_activate(self):
        self._file_loader = CausewayFileLoader()
        self.application.add_file_loader(self._file_loader)

    def do_deactivate(self):
        self.application.remove_file_loader(self._file_loader)
        self._file_loader = None
