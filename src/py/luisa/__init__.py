from .dylibs import lcapi
from . import globalvars
from .types import half, short, ushort, half2, short2, ushort2, half3, short3, ushort3, half4, short4, ushort4

from .func import func, save_raster_shader
from .mathtypes import *
from .array import array, ArrayType, SharedArrayType
from .struct import struct, StructType
from .buffer import buffer, Buffer, BufferType, DispatchIndirectBuffer
from .image2d import image2d, Image2D, Texture2DType
from .image3d import image3d, Image3D, Texture3DType
from .dylibs.lcapi import PixelStorage
from .gui import GUI

from .printer import Printer
from .accel import Accel, make_ray, inf_ray, offset_ray_origin
from .hit import TriangleHit, CommittedHit, ProceduralHit
from .rayquery import RayQueryAllType, RayQueryAnyType, is_triangle, is_procedural, Ray
from .bindless import bindless_array, BindlessArray
from .util import RandomSampler
from .meshformat import MeshFormat

from .dylibs.lcapi import log_level_verbose, log_level_info, log_level_warning, log_level_error
from os.path import realpath
import platform



def init(backend_name = None, shader_path = None, support_gui = True):
    if globalvars.device is not None:
        return
    if globalvars.context is None:
        globalvars.context = lcapi.Context(realpath(lcapi.__file__))
    # auto select backend if not specified
    backends = globalvars.context.installed_backends()
    assert len(backends) > 0
    if backend_name is None:
        platform_str = str(platform.platform()).lower()
        if platform_str.find("windows") >= 0:
            for i in backends:
                if i == "dx":
                    backend_name = "dx"
                    break
            if backend_name is None:
                backend_name = backends[0]
        elif platform_str.find("linux") >= 0:
            for i in backends:
                if i == "cuda":
                    backend_name = "cuda"
                    break
            if backend_name is None:
                backend_name = backends[0]
        elif platform_str.find("macos") >= 0:
            for i in backends:
                if i == "metal":
                    backend_name = "metal"
                    break
            if backend_name is None:
                backend_name = backends[0]
        else:
            backend_name = backends[0]
        print(f"detected backends: {backends}. Selecting {backend_name}.")
        
    elif backend_name not in backends:
        raise NameError(f"backend '{backend_name}' is not installed.")
    globalvars.device = globalvars.context.create_device(backend_name)
    globalvars.stream = globalvars.device.create_stream(support_gui)
    globalvars.stream_support_gui = support_gui
    if shader_path is not None:
        globalvars.context.set_shader_path(shader_path)

def init_headless(backend_name = None, shader_path = None):
    if globalvars.device is not None:
        return
    if globalvars.context is None:
        globalvars.context = lcapi.Context(realpath(lcapi.__file__))
    # auto select backend if not specified
    backends = globalvars.context.installed_backends()
    assert len(backends) > 0
    if backend_name is None:
        print(f"detected backends: {backends}. Selecting {backends[0]}.")
        backend_name = backends[0]
    elif backend_name not in backends:
        raise NameError(f"backend '{backend_name}' is not installed.")
    globalvars.device = globalvars.context.create_headless_device(backend_name)
    if shader_path is not None:
        globalvars.context.set_shader_path(shader_path)
def del_device():
    if globalvars.device is not None:
        del globalvars.device
        globalvars.device = None

def synchronize(stream = None):
    if stream is None:
        stream = globalvars.stream
    stream.synchronize()

def execute(stream = None):
    if stream is None:
        stream = globalvars.stream
    stream.execute()