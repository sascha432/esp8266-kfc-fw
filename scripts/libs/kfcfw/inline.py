

#
# create object from dictionary
# starting with _ is private and won't show up using print(obj)
#
class Inline(object):
    def __init__(self, **args):
        self.__dict__.update(args)

    def __str__(self):
        parts = []
        for key, val in self.__dict__.items():
            if not key.startswith('_'):
                if val==None:
                    val = 'None'
                if isinstance(val, str):
                    val = "'" + val + "'"
                else:
                    val = str(val)
                parts.append(format('%s = %s' % (key, val)))
        return '(%s)' % ', '.join(parts)

#
# create object with UPPERCASE names from dictionary
#
class InlineConst(Inline):
    def __init__(self, **args):
        d = {}
        value = 0
        for key, val in args.items():
            if isinstance(val, list):
                for key in val:
                    d[str(key).upper()] = value
                    value += 1
            else:
                if isinstance(val, str):
                    val = d[val.upper()]
                elif not val:
                    val = value
                else:
                    val = int(val)
                if not val and value:
                    val = value
                d[str(key).upper()] = val
                value = val + 1

        self.__dict__.update(d)

#
# enumeration
# takes
#

class InlineEnum(InlineConst):
    def __init__(self, **args):
        InlineConst.__init__(self, **args)
        self._value = None

    def __getitem__(self, value):
        for key, val in self.__dict__.items():
            if key == value:
                return val
        raise KeyError(value)

    def __str__(self):
        for key, val in self.__dict__.items():
            if val == self._value:
                return key
        return None

    def __int__(self):
        return self._value

    def set(self, value):
        if isinstance(value, int):
            if not value in self.__dict__.values():
                raise ValueError(value)
            self._value = value
        elif isinstance(value, str):
            self._value = self.__getitem__(value.upper())
        else:
            raise TypeError(value)

    def get(self):
        return self.__int__()

    def __repr__(self):
        return '%s %s' % (self._value, InlineConst.__str__(self))

    def __eq__(self, value):
        if isinstance(value, int):
            return value == self._value
        elif isinstance(value, str):
            return self.__getitem__(value.upper()) == self._value
        return False



# enum = InlineEnum(
#     __auto1 = ['a','b','c'],        # enumerate starting with 0
#     d = 10,                         # argument with value
#     e = None,                       # argument with auto value
#     f = 15,
#     __auto2 = ['x', 'y', 'z']       # more enumration
# )
# print('repr', enum.__repr__())
# enum.set(enum.Z)
# print('enum.set(enum.Z):', 'str', enum, 'int', int(enum), 'get', enum.get())
# enum.set('f')
# print("enum.set('f'):", 'str', enum, 'int', int(enum), 'get', enum.get())

