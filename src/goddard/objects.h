#ifndef GD_OBJECTS_H
#define GD_OBJECTS_H

#include <PR/ultratypes.h>

#include "gd_types.h"
#include "macros.h"

// types
// Type to erase for func arg to `apply_to_obj_types_in_group`. Maybe one day this
// can be the proper type of 'void (*)(struct GdObj *)...
typedef void (*applyproc_t)(void *);

// structs
struct DebugCounters {
    u32 ctr0;
    s32 ctr1;
};

// bss
extern struct GdPlaneF D_801B9DA0;
extern struct ObjCamera *sCurrentMoveCamera;
extern struct ObjView *sCurrentMoveView;
extern struct DebugCounters gGdCounter;
extern Mat4f D_801B9DC8;
extern struct GdVec3f D_801B9E08;
extern struct ObjGroup* sCurrentMoveGrp;
extern struct GdVec3f D_801B9E18;
extern struct GdVec3f D_801B9E28;
extern f32 D_801B9E34;
extern Mat4f* D_801B9E38;                /* never read from */
extern struct ObjParticle *D_801B9E3C;  /* never read from */
extern s32 D_801B9E40;                  /* always 0 */
extern s32 D_801B9E44;
extern Mat4f* D_801B9E48;
extern struct ObjCamera* gGdCameraList;
extern void* D_801B9E50;
extern struct ObjGroup* gGdGroupList;
extern s32 gGdObjCount;
extern s32 gGdGroupCount;
extern s32 gGdPlaneCount;
extern s32 gGdCameraCount;
extern struct Unk801B9E68 sGdViewInfo;   /* count in first member? */
extern void* D_801B9E80;
extern struct ObjJoint* gGdJointList;
extern struct ObjBone* gGdBoneList;
extern struct GdObj* gGdObjectList;
extern struct ObjGroup* gGdViewsGroup;

// functions
void func_8017BCB0(void);
void func_8017BD20(void *a0);
void func_8017BE60(struct GdPlaneF *a0);
const char *get_obj_name_str(enum ObjTypeFlag objFlag);
struct GdObj *make_object(enum ObjTypeFlag objFlag);
struct ObjZone *make_zone(struct ObjGroup *a0, struct GdPlaneF *a1, struct ObjGroup *a2);
struct ObjUnk200000 *func_8017C7A0(struct ObjVertex *a0, struct ObjFace *a1);
struct Links *make_link_to_obj(struct Links *head, struct GdObj *a1);
struct VtxLink *make_vtx_link(struct VtxLink *prevLink, Vtx *data);
struct ObjValPtrs *make_valptrs(struct GdObj *obj, s32 flags, enum ValPtrType type, size_t offset);
void reset_plane(struct ObjPlane *plane);
struct ObjPlane *make_plane(s32 inZone, struct ObjFace *a1);
struct ObjCamera *make_camera(s32 a0, struct GdObj *a1);
struct ObjMaterial *make_material(UNUSED s32 a0, char *name, s32 id);
struct ObjLight *make_light(s32 flags, char *name, s32 id);
struct ObjView *make_view(const char *name, s32 flags, s32 a2, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          struct ObjGroup *parts);
struct ObjAnimator *make_animator(void);
struct ObjWeight *make_weight(UNUSED s32 a0, s32 id, struct ObjVertex *vtx, f32 weight);
struct ObjGroup *make_group_of_type(enum ObjTypeFlag, struct GdObj*, struct GdObj*);
void sprint_obj_id(char*, struct GdObj*);
struct ObjGroup* make_group(s32 count, ...);
void addto_group(struct ObjGroup *group, struct GdObj *obj);
void addto_groupfirst(struct ObjGroup *group, struct GdObj *obj);
s32 group_contains_obj(struct ObjGroup *group, struct GdObj *obj);
void show_details(enum ObjTypeFlag type);
s32 func_8017E1E8(void);
s32 func_8017E20C(void);
void gd_loadtexture(struct GdObj *obj);
void func_8017E2B8(void);
struct GdObj *func_8017E2F0(struct GdObj *obj, enum ObjTypeFlag type);
s32 apply_to_obj_types_in_group(s32 types, applyproc_t fn, struct ObjGroup *group);
void func_8017E584(struct ObjNet *a0, struct GdVec3f *a1, struct GdVec3f *a2);
void func_8017E838(struct ObjNet *a0, struct GdVec3f *a1, struct GdVec3f *a2);
void func_8017E9EC(struct ObjNet *a0);
s32 func_8017EA94(struct GdVec3f *vec, Mat4f matrix);
s32 func_8017EB24(struct GdObj *a0, struct GdObj *a1);
s32 func_8017ED00(struct GdObj *a0, struct GdPlaneF *a1);
s32 func_8017EDCC(struct GdVec3f *a0, struct GdPlaneF *a1);
s32 gd_plane_point_within(struct GdPlaneF *a0, struct GdPlaneF *a1);
s32 func_8017F054(struct GdObj *a0, struct GdObj *a1);
s32 func_8017F210(struct GdObj *a0, struct GdObj *a1);
void func_8017F404(UNUSED f32 a0, UNUSED struct GdObj *a1, UNUSED struct GdObj *a2);
void func_8017F424(struct GdTriangleF *a0, struct GdTriangleF *a1, f32 a2);
void move_animator(struct ObjAnimator *animObj);
void drag_picked_object(struct GdObj *inputObj);
void move_animators(struct ObjGroup *group);
void find_and_drag_picked_object(struct ObjGroup *group);
void move_camera(struct ObjCamera *cam);
void move_cameras_in_grp(struct ObjGroup *group);
void func_8018100C(struct ObjLight *light);
void move_lights_in_grp(struct ObjGroup *group);
void proc_view_movement(struct ObjView *view);
void reset_nets_and_gadgets(struct ObjGroup *group);
void null_obj_lists(void);

#endif // GD_OBJECTS_H
