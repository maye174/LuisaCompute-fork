use std::os::raw::c_void;

use super::resource::BufferImpl;
use crate::panic_abort;
use api::{
    AccelBuildModification, AccelBuildModificationFlags, AccelBuildRequest, AccelUsageHint,
    MeshBuildCommand,
};
use embree_sys as sys;
use lazy_static::lazy_static;
use luisa_compute_api_types as api;
use luisa_compute_cpu_kernel_defs as defs;
use parking_lot::{Mutex, RwLock};
struct Device(sys::RTCDevice);
unsafe impl Send for Device {}
unsafe impl Sync for Device {}

lazy_static! {
    static ref DEVICE: Mutex<Device> = Mutex::new(Device(std::ptr::null_mut()));
}
fn init_device() {
    let mut device = DEVICE.lock();
    if device.0.is_null() {
        device.0 = unsafe { sys::rtcNewDevice(std::ptr::null()) }
    }
}
pub struct MeshImpl {
    pub(crate) handle: sys::RTCScene,
    usage: AccelUsageHint,
    built: bool,
    lock: Mutex<()>,
}
macro_rules! check_error {
    ($device:expr) => {{
        let err = sys::rtcGetDeviceError($device);
        if err != sys::RTC_ERROR_NONE {
            panic_abort!("Embree error: {}", err);
        }
    }};
}
impl MeshImpl {
    pub unsafe fn new(
        hint: api::AccelUsageHint,
        _allow_compact: bool,
        _allow_update: bool,
    ) -> Self {
        init_device();
        let device = DEVICE.lock();
        let handle = sys::rtcNewScene(device.0);
        let flags = match hint {
            AccelUsageHint::FastBuild => sys::RTC_BUILD_QUALITY_LOW,
            AccelUsageHint::FastTrace => sys::RTC_BUILD_QUALITY_HIGH,
        };

        sys::rtcSetSceneFlags(handle, flags);

        Self {
            handle,
            usage: hint,
            built: false,
            lock: Mutex::new(()),
        }
    }
    pub unsafe fn build(&mut self, cmd: &MeshBuildCommand) {
        let device = DEVICE.lock();
        let device = device.0;
        let _lk = self.lock.lock();
        let request = cmd.request;
        let need_rebuild = request == AccelBuildRequest::ForceBuild || !self.built;

        if need_rebuild {
            let geometry = sys::rtcNewGeometry(device, sys::RTC_GEOMETRY_TYPE_TRIANGLE);
            let vbuffer = &*(cmd.vertex_buffer.0 as *const BufferImpl);
            let ibuffer = &*(cmd.index_buffer.0 as *const BufferImpl);
            sys::rtcSetSharedGeometryBuffer(
                geometry,
                sys::RTC_BUFFER_TYPE_VERTEX,
                0,
                sys::RTC_FORMAT_FLOAT3,
                vbuffer.data as *const c_void,
                0,
                cmd.vertex_stride,
                cmd.vertex_buffer_size / cmd.vertex_stride,
            );
            check_error!(device);
            sys::rtcSetSharedGeometryBuffer(
                geometry,
                sys::RTC_BUFFER_TYPE_INDEX,
                0,
                sys::RTC_FORMAT_UINT3,
                ibuffer.data as *const c_void,
                0,
                cmd.index_stride,
                cmd.index_buffer_size / cmd.index_stride,
            );
            check_error!(device);
            sys::rtcCommitGeometry(geometry);
            check_error!(device);
            if self.built {
                sys::rtcDetachGeometry(self.handle, 0);
                check_error!(device);
            } else {
                self.built = true;
            }
            sys::rtcAttachGeometryByID(self.handle, geometry, 0);
            check_error!(device);
            sys::rtcReleaseGeometry(geometry);
            check_error!(device);
        } else {
            let geometry = sys::rtcGetGeometry(self.handle, 0);
            sys::rtcUpdateGeometryBuffer(geometry, sys::RTC_BUFFER_TYPE_VERTEX, 0);
            check_error!(device);
            sys::rtcCommitGeometry(geometry);
            check_error!(device);
        }
        sys::rtcCommitScene(self.handle);
        check_error!(device);
    }
}
impl Drop for MeshImpl {
    fn drop(&mut self) {
        unsafe {
            sys::rtcReleaseScene(self.handle);
        }
    }
}
struct Instance {
    affine: [f32; 12],
    dirty: bool,
    visible: u8,
    geometry: sys::RTCGeometry,
}
impl Instance {
    pub fn valid(&self) -> bool {
        self.geometry != std::ptr::null_mut()
    }
}
impl Default for Instance {
    fn default() -> Self {
        Self {
            affine: [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0],
            dirty: false,
            visible: u8::MAX,
            geometry: std::ptr::null_mut(),
        }
    }
}
pub struct AccelImpl {
    pub(crate) handle: sys::RTCScene,
    instances: Vec<RwLock<Instance>>,
}
#[derive(Clone, Copy)]
#[repr(C)]
struct RayQueryContext {
    parent: sys::RTCRayQueryContext,
    rq: *mut defs::RayQuery,
    on_triangle_hit: defs::OnHitCallback,
    on_procedural_hit: defs::OnHitCallback,
}
impl AccelImpl {
    pub unsafe fn new() -> Self {
        init_device();
        let device = DEVICE.lock();
        let handle = sys::rtcNewScene(device.0);
        Self {
            handle,
            instances: Vec::new(),
        }
    }
    pub unsafe fn update(
        &mut self,
        instance_count: usize,
        modifications: &[AccelBuildModification],
    ) {
        let device = DEVICE.lock();
        let device = device.0;
        while instance_count > self.instances.len() {
            self.instances.push(RwLock::new(Instance::default()));
        }
        while instance_count < self.instances.len() {
            let last = self.instances.pop().unwrap().into_inner();
            if last.valid() {
                sys::rtcDetachGeometry(self.handle, self.instances.len() as u32);
            }
        }
        for m in modifications {
            if m.flags.contains(AccelBuildModificationFlags::PRIMITIVE) {
                let mesh = &*(m.mesh as *const MeshImpl);
                assert!(mesh.built);
                unsafe {
                    let affine = m.affine;
                    let geometry = sys::rtcNewGeometry(device, sys::RTC_GEOMETRY_TYPE_INSTANCE);
                    sys::rtcCommitGeometry(geometry);
                    sys::rtcSetGeometryInstancedScene(geometry, mesh.handle);
                    sys::rtcAttachGeometryByID(self.handle, geometry, m.index);
                    sys::rtcSetGeometryEnableFilterFunctionFromArguments(geometry, true);
                    *self.instances[m.index as usize].write() = Instance {
                        affine,
                        dirty: false,
                        visible: u8::MAX,
                        geometry,
                    };
                }
            }
            if m.flags.contains(AccelBuildModificationFlags::TRANSFORM) {
                let mut instance = self.instances[m.index as usize].write();
                let geometry = instance.geometry;
                assert!(!geometry.is_null());
                let affine = m.affine;
                sys::rtcSetGeometryTransform(
                    geometry,
                    0,
                    sys::RTC_FORMAT_FLOAT3X4_ROW_MAJOR,
                    affine.as_ptr() as *const c_void,
                );
                instance.affine = affine;
                instance.dirty = true;
            }
            if m.flags.contains(AccelBuildModificationFlags::VISIBILITY) {
                let mut instance = self.instances[m.index as usize].write();
                let geometry = instance.geometry;
                assert!(!geometry.is_null());
                sys::rtcEnableGeometry(geometry);
                sys::rtcSetGeometryMask(geometry, m.visibility as u32);
                instance.visible = m.visibility;
                instance.dirty = true;
            }
        }
        for instance in &self.instances {
            let mut instance = instance.write();
            if instance.valid() && instance.dirty {
                sys::rtcCommitGeometry(instance.geometry);
                instance.dirty = false;
            }
        }
        sys::rtcCommitScene(self.handle);
    }
    #[inline]
    pub unsafe fn trace_closest(&self, ray: &defs::Ray, mask: u8) -> defs::Hit {
        let mut rayhit = sys::RTCRayHit {
            ray: sys::RTCRay {
                org_x: ray.orig_x,
                org_y: ray.orig_y,
                org_z: ray.orig_z,
                tnear: ray.tmin,
                dir_x: ray.dir_x,
                dir_y: ray.dir_y,
                dir_z: ray.dir_z,
                time: 0.0,
                tfar: ray.tmax,
                mask: mask as u32,
                id: 0,
                flags: 0,
            },
            hit: sys::RTCHit {
                Ng_x: 0.0,
                Ng_y: 0.0,
                Ng_z: 0.0,
                u: 0.0,
                v: 0.0,
                primID: u32::MAX,
                geomID: u32::MAX,
                instID: [u32::MAX],
            },
        };
        let mut args = sys::RTCIntersectArguments {
            flags: sys::RTC_RAY_QUERY_FLAG_INCOHERENT,
            feature_mask: sys::RTC_FEATURE_FLAG_ALL,
            filter: None,
            intersect: None,
            context: std::ptr::null_mut(),
        };

        sys::rtcIntersect1(self.handle, &mut rayhit as *mut _, &mut args as *mut _);
        if rayhit.hit.geomID != u32::MAX && rayhit.hit.primID != u32::MAX {
            defs::Hit {
                inst_id: rayhit.hit.instID[0],
                prim_id: rayhit.hit.primID,
                u: rayhit.hit.u,
                v: rayhit.hit.v,
                t: rayhit.ray.tfar,
            }
        } else {
            defs::Hit {
                inst_id: u32::MAX,
                prim_id: u32::MAX,
                u: 0.0,
                v: 0.0,
                t: ray.tmax,
            }
        }
    }
    #[inline]
    pub unsafe fn trace_any(&self, ray: &defs::Ray, mask: u8) -> bool {
        let mut ray = sys::RTCRay {
            org_x: ray.orig_x,
            org_y: ray.orig_y,
            org_z: ray.orig_z,
            tnear: ray.tmin,
            dir_x: ray.dir_x,
            dir_y: ray.dir_y,
            dir_z: ray.dir_z,
            time: 0.0,
            tfar: ray.tmax,
            mask: mask as u32,
            id: 0,
            flags: 0,
        };
        let mut args = sys::RTCOccludedArguments {
            flags: sys::RTC_RAY_QUERY_FLAG_INCOHERENT,
            feature_mask: sys::RTC_FEATURE_FLAG_ALL,
            filter: None,
            occluded: None,
            context: std::ptr::null_mut(),
        };
        sys::rtcOccluded1(self.handle, &mut ray as *mut _, &mut args as *mut _);
        ray.tfar < 0.0
    }
    #[inline]
    pub unsafe fn instance_transform(&self, id: u32) -> [f32; 12] {
        let geometry = sys::rtcGetGeometry(self.handle, id);
        let mut affine = [0.0; 12];
        sys::rtcGetGeometryTransform(
            geometry,
            0.0,
            sys::RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR,
            affine.as_mut_ptr() as *mut c_void,
        );
        affine
    }
    #[inline]
    pub unsafe fn set_instance_transform(&self, id: u32, affine: [f32; 12]) {
        let mut instance = self.instances[id as usize].write();
        assert!(instance.valid());
        instance.affine = affine;
    }
    #[inline]
    pub unsafe fn set_instance_visibility(&self, id: u32, visibility: u8) {
        let mut instance = self.instances[id as usize].write();
        assert!(instance.valid());
        instance.visible = visibility;
    }

    #[inline]
    pub unsafe fn ray_query(
        &self,
        rq: &mut defs::RayQuery,
        on_triangle_hit: defs::OnHitCallback,
        on_procedural_hit: defs::OnHitCallback,
    ) {
        let ray = rq.ray;
        let mut ray = sys::RTCRay {
            org_x: ray.orig_x,
            org_y: ray.orig_y,
            org_z: ray.orig_z,
            tnear: ray.tmin,
            dir_x: ray.dir_x,
            dir_y: ray.dir_y,
            dir_z: ray.dir_z,
            time: 0.0,
            tfar: ray.tmax,
            mask: rq.mask as u32,
            id: 0,
            flags: 0,
        };
        let mut ctx = RayQueryContext {
            parent: sys::RTCRayQueryContext {
                instID: [u32::MAX; 1],
            },
            rq,
            on_procedural_hit,
            on_triangle_hit,
        };

        /* Helper functions to access ray packets of runtime size N */
        // RTC_FORCEINLINE float& RTCRayN_org_x(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[0*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_org_y(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[1*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_org_z(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[2*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_tnear(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[3*N+i]; }

        // RTC_FORCEINLINE float& RTCRayN_dir_x(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[4*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_dir_y(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[5*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_dir_z(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[6*N+i]; }
        // RTC_FORCEINLINE float& RTCRayN_time (RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[7*N+i]; }

        // RTC_FORCEINLINE float&        RTCRayN_tfar (RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[8*N+i]; }
        // RTC_FORCEINLINE unsigned int& RTCRayN_mask (RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[9*N+i]; }
        // RTC_FORCEINLINE unsigned int& RTCRayN_id   (RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[10*N+i]; }
        // RTC_FORCEINLINE unsigned int& RTCRayN_flags(RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[11*N+i]; }
        /* Helper functions to access hit packets of runtime size N */
        // RTC_FORCEINLINE float& RTCHitN_Ng_x(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[0*N+i]; }
        // RTC_FORCEINLINE float& RTCHitN_Ng_y(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[1*N+i]; }
        // RTC_FORCEINLINE float& RTCHitN_Ng_z(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[2*N+i]; }

        // RTC_FORCEINLINE float& RTCHitN_u(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[3*N+i]; }
        // RTC_FORCEINLINE float& RTCHitN_v(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[4*N+i]; }

        // RTC_FORCEINLINE unsigned int& RTCHitN_primID(RTCHitN* hit, unsigned int N, unsigned int i) { return ((unsigned*)hit)[5*N+i]; }
        // RTC_FORCEINLINE unsigned int& RTCHitN_geomID(RTCHitN* hit, unsigned int N, unsigned int i) { return ((unsigned*)hit)[6*N+i]; }
        // RTC_FORCEINLINE unsigned int& RTCHitN_instID(RTCHitN* hit, unsigned int N, unsigned int i, unsigned int l) { return ((unsigned*)hit)[7*N+i+N*l]; }

        // /* Helper functions to extract RTCRayN and RTCHitN from RTCRayHitN */
        // RTC_FORCEINLINE RTCRayN* RTCRayHitN_RayN(RTCRayHitN* rayhit, unsigned int N) { return (RTCRayN*)&((float*)rayhit)[0*N]; }
        // RTC_FORCEINLINE RTCHitN* RTCRayHitN_HitN(RTCRayHitN* rayhit, unsigned int N) { return (RTCHitN*)&((float*)rayhit)[12*N]; }
        unsafe extern "C" fn filter_fn(args: *const sys::RTCFilterFunctionNArguments) {
            let args = &*args;
            let ctx = &mut *(args.context as *mut RayQueryContext);
            debug_assert!(args.N == 1);
            let hit = args.hit as *mut u32;
            let prim_id = *hit.add(5);
            let geom_id = *hit.add(6);
            let inst_id = *hit.add(7);
            let hit_u = *(args.hit as *mut f32).add(3);
            let hit_v = *(args.hit as *mut f32).add(4);
            let t_far = &mut *(args.ray as *mut f32).add(8);
            debug_assert!(prim_id != u32::MAX && geom_id != u32::MAX);
            let rq = &mut *ctx.rq;
            rq.cur_triangle_hit = defs::TriangleHit {
                prim: prim_id,
                inst: inst_id,
                bary: [hit_u, hit_v],
                committed_ray_t: *t_far,
            };
            let on_triangle_hit = ctx.on_triangle_hit;
            rq.cur_commited = false;
            rq.terminated = false;
            on_triangle_hit(rq);
            if !rq.cur_commited {
                *args.valid = 0;
            } else {
                // eprintln!("accepting hit");
                rq.hit.set_from_triangle_hit(rq.cur_triangle_hit);
            }
            if rq.terminated {
                *t_far = f32::NEG_INFINITY;
            }
        }
        unsafe extern "C" fn occluded_fn(args: *const sys::RTCOccludedFunctionNArguments) {}
        if rq.terminate_on_first {
            let mut args = sys::RTCOccludedArguments {
                flags: sys::RTC_RAY_QUERY_FLAG_INCOHERENT
                    | sys::RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER,
                feature_mask: sys::RTC_FEATURE_FLAG_ALL,
                filter: Some(filter_fn),
                occluded: None,
                context: &mut ctx.parent as *mut _,
            };
            sys::rtcOccluded1(self.handle, &mut ray as *mut _, &mut args as *mut _);
        } else {
            let mut rayhit = sys::RTCRayHit {
                ray,
                hit: sys::RTCHit {
                    Ng_x: 0.0,
                    Ng_y: 0.0,
                    Ng_z: 0.0,
                    u: 0.0,
                    v: 0.0,
                    primID: u32::MAX,
                    geomID: u32::MAX,
                    instID: [u32::MAX],
                },
            };
            let mut args = sys::RTCIntersectArguments {
                flags: sys::RTC_RAY_QUERY_FLAG_INCOHERENT
                    | sys::RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER,
                feature_mask: sys::RTC_FEATURE_FLAG_ALL,
                filter: Some(filter_fn),
                intersect: None,
                context: &mut ctx.parent as *mut _,
            };

            sys::rtcIntersect1(self.handle, &mut rayhit as *mut _, &mut args as *mut _);
        }
    }
}

impl Drop for AccelImpl {
    fn drop(&mut self) {
        unsafe {
            sys::rtcReleaseScene(self.handle);
        }
    }
}
