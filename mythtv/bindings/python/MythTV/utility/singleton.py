#------------------------------
# MythTV/utility/singleton.py
# Author: Raymond Wagner
# Description: Provides multiple singleton types
#------------------------------

class Singleton( type ):
    """
    Basic singleton metaclass.
    Only allows a single instance of the class.
    """
    def __new__(mcs, name, bases, kwargs):
        if '__slots__' in kwargs:
            kwargs['__slots__']['_{0}__instance'.format(name)] = None
        else:
            kwargs['_{0}__instance'.format(name)] = None
        return type.__new__(mcs, name, bases, kwargs)

    def __call__(cls, *args, **kwargs):
        attr = '_{0}__instance'.format(cls.__name__)
        inst = getattr(cls, attr)
        if inst:
            return inst
        inst = type.__call__(cls, *args, **kwargs)
        setattr(cls, attr, inst)
        return inst

class InputSingleton( type ):
    """
    Advanced singleton metaclass.
    Allows multiple instances of the class, indexed by the inputs.
    """
    def __new__(mcs, name, bases, kwargs):
        if '__slots__' in kwargs:
            kwargs['__slots__']['_{0}__instance'.format(name)] = {}
        else:
            kwargs['_{0}__instance'.format(name)] = {}
        return type.__new__(mcs, name, bases, kwargs)

    def __call__(cls, *args, **kwargs):
        attr = '_{0}__instance'.format(cls.__name__)
        insts = getattr(cls, attr)

        from inspect import getcallargs
        instid = hash(str(getcallargs(cls.__init__, args, kwargs)))

        if instid in insts:
            return insts[instid]
        inst = type.__call__(cls, *args, **kwargs)
        insts[instid] = inst
        return inst

class CmpSingleton( type ):
    """
    Advanced singleton metaclass.
    Allows multiple instances of the class, comparing object after creation.
    """
    def __new__(mcs, name, bases, kwargs):
        if '__slots__' in kwargs:
            kwargs['__slots__']['_{0}__instance'.format(name)] = []
        else:
            kwargs['_{0}__instance'.format(name)] = []
        return type.__new__(mcs, name, bases, kwargs)

    def __call__(cls, *args, **kwargs):
        attr = '_{0}__instance'.format(cls.__name__)
        insts = getattr(cls, attr)
        obj = type.__call__(cls, *args, **kwargs)
        for inst in insts:
            if obj == inst:
                return inst
        inst.append(obj)
        return obj

