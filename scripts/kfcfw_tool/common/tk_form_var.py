#
# Author: sascha_lammers@gmx.de
#

import tkinter

#
# creates a dictionary from one or more dictionaries that returns
# String(Int/Double/Boolear)Var for the use form widgets. the data is
# synchronized between the dictionaries and widgets automatically
#
# customer = { 'first': 'John', 'last': 'Doe' }
# details = { 'newsletter': False, 'deleted': False }
# data = TkFormVar(customer, details)
#
# tk.Entry(self, textvariable=data['first'])
# tk.Entry(self, textvariable=data['last'])
# tk.Checkbutton(self, variable=data['newsletter'])
# tk.Checkbutton(self, variable=data['deleted'])

# trace reads, writes, undefines, validations and modified validations

# self.form = tk_form_var.TkFormVar(self.config)
# self.form.trace('m', callback=lambda event, key, val, *args: print('%s has been modified: new value=%s' % (key, val)))
# self.form.trace('v', callback=lambda event, key, *args: print(event, key, args))
# self.form.trace('rwu', callback=lambda event, key, *args: print(event, key, args))


class TkFormVar(object):
    def __init__(self, *dicts):
        self._dicts = dicts
        self._tkVars = {}
        self._callbacks = { 'w': self.__nocallback__, 'r': self.__nocallback__, 'u': self.__nocallback__, 'v': self.__nocallback__ , 'm': self.__nocallback__ }
        self._modified = {}

    def __nocallback__(self, *args):
        pass

    def trace(self, what, callback = None):
        if len(what)>1:
            for w in what:
                self.trace(w, callback)
            return
        if what not in 'rwuvm':
            raise KeyError('callback does not exist: %s' % what)
        if callback==None:
            self._callbacks[what] = self.__nocallback__
        else:
            self._callbacks[what] = callback

    def __getitem__(self, key):
        if key in self._tkVars:
            return self._tkVars[key]
        value = self.__dicts_get_item(key)
        if value==None:
            raise KeyError('key does not exist: %s' % key)

        if isinstance(value, int):
            var = tkinter.IntVar()
        elif isinstance(value, bool):
            var = tkinter.BooleanVar()
        elif isinstance(value, float):
            var = tkinter.DoubleVar()
        elif isinstance(value, str):
            var = tkinter.StringVar()
        else:
            raise TypeError('type not supported: %s' % type(value))

        for num, _dict in enumerate(self._dicts):
            if key in _dict:
                # var.trace('r', callback=lambda *args,value=_dict[key],var=var: var.set(value))
                var.trace('r', callback=lambda *args,num=num,key=key,var=var: self.__dicts_read(num, key, var))
                var.trace('w', callback=lambda *args,num=num,key=key,var=var: self.__dicts_write(num, key, var))
                var.trace('u', callback=lambda *args,num=num,key=key,var=var: self.__dicts_undef(num, key, var))
        self._tkVars[key] = var
        return var

    def __dicts_read(self, num, key, var):
        try:
            value = self._dicts[num][key]
            var.set(value)
            self._modified[key] = value
        except tkinter.TclError as e:
            pass
        self._callbacks['r']('r', key, self._dicts[num][key], var)

    def __dicts_undef(self, num, key, var):
        try:
            del self._dicts[num][key]
            del self._modified[key]
        except:
            pass
        self._callbacks['u']('u', key, None, var)

    def __dicts_write(self, num, key, var):
        try:
            self._dicts[num][key] = var.get()
            value = self._dicts[num][key]
        except tkinter.TclError as e:
            value = None
            pass
        self._callbacks['w']('w', key, value, var)

    def __dicts_get_item(self, key):
        for _dict in self._dicts:
            if key in _dict:
                return _dict[key]
        return None

    def __contains__(self, key):
        return self.__dicts_get_item(key) != None

    def __len__(self):
        return len(self._tkVars)

    def __repr__(self):
        return repr(self._tkVars)

    def __validatecommand__(self, entry, key):
        val = self.__dicts_get_item(key)
        modified = False
        try:
            old_val = self._modified[key]
        except:
            old_val = None
        modified = old_val!=val
        self._modified[key] = val
        self._callbacks['v']('v', key, val, self._tkVars[key], modified)
        if modified:
            self._callbacks['m']('m', key, val, self._tkVars[key])
        return True

    def set_textvariable(self, entry, key, validate = False):
        if validate==True:
            entry.configure(textvariable=self[key], validate="focusout", validatecommand=lambda: self.__validatecommand__(entry, key))
        else:
            entry.configure(textvariable=self)
        return entry
