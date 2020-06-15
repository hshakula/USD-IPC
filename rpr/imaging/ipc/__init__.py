from . import _ipc
from pxr import Tf
Tf.PrepareModule(_ipc, locals())
del Tf

try:
    from . import __DOC
    __DOC.Execute(locals())
    del __DOC
except Exception:
    pass
