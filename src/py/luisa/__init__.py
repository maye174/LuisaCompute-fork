import lcapi
from . import globalvars
from .types import ref

from .kernel import kernel, callable, callable_method
from .mathtypes import *
from .arraytype import ArrayType
from .structtype import StructType
from .buffer import Buffer, BufferType
from .texture2d import Texture2D, Texture2DType
from lcapi import PixelStorage

from .printer import Printer
from .accel import Ray, Hit, Accel, Mesh


def init(backend_name = None):
    globalvars.context = lcapi.Context(lcapi.FsPath("."))
    # auto select backend if not specified
    if backend_name == None:
        backends = globalvars.context.installed_backends()
        assert len(backends) > 0
        print("detected backends:", backends, "selecting first one.")
        backend_name = backends[0]
    globalvars.device = globalvars.context.create_device(backend_name)
    globalvars.stream = globalvars.device.create_stream()
    globalvars.printer = Printer()


def synchronize(stream = None):
    if stream is None:
        stream = globalvars.stream
    stream.synchronize()

