/*
 * Copyright (c) 2008 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Make sure that XTHREADS is defined, so that the
 * LockDisplay/UnlockDisplay macros are expanded properly and the
 * libXNVCtrl library properly protects the Display connection.
 */

#if !defined(XTHREADS)
#define XTHREADS
#endif /* XTHREADS */

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include "NVCtrlLib.h"
#include "nv_control.h"

#define NVCTRL_EXT_NEED_CHECK          (XPointer)(~0)
#define NVCTRL_EXT_NEED_NOTHING        (XPointer)(0)
#define NVCTRL_EXT_NEED_TARGET_SWAP    (XPointer)(1)

static XExtensionInfo _nvctrl_ext_info_data;
static XExtensionInfo *nvctrl_ext_info = &_nvctrl_ext_info_data;
static /* const */ char *nvctrl_extension_name = NV_CONTROL_NAME;

#define XNVCTRLCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, nvctrl_extension_name, val)
#define XNVCTRLSimpleCheckExtension(dpy,i) \
  XextSimpleCheckExtension (dpy, i, nvctrl_extension_name)

static int close_display();
static Bool wire_to_event();
static /* const */ XExtensionHooks nvctrl_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    wire_to_event,                      /* wire_to_event */
    NULL,                               /* event_to_wire */
    NULL,                               /* error */
    NULL,                               /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, nvctrl_ext_info,
                                   nvctrl_extension_name, 
                                   &nvctrl_extension_hooks,
                                   NV_CONTROL_EVENTS, NVCTRL_EXT_NEED_CHECK)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, nvctrl_ext_info)

/*
 * NV-CONTROL versions 1.8 and 1.9 pack the target_type and target_id
 * fields in reversed order.  In order to talk to one of these servers,
 * we need to swap these fields.
 */
static void XNVCTRLCheckTargetData(Display *dpy, XExtDisplayInfo *info,
                                   int *target_type, int *target_id)
{
    /* Find out what the server's NV-CONTROL version is and
     * setup for swapping if we need to.
     */
    if (info->data == NVCTRL_EXT_NEED_CHECK) {
        int major, minor;

        if (XNVCTRLQueryVersion(dpy, &major, &minor)) {
            if (major == 1 &&
                (minor == 8 || minor == 9)) {
                info->data = NVCTRL_EXT_NEED_TARGET_SWAP;
            } else {
                info->data = NVCTRL_EXT_NEED_NOTHING;
            }
        } else {
            info->data = NVCTRL_EXT_NEED_NOTHING;
        }
    }

    /* We need to swap the target_type and target_id */
    if (info->data == NVCTRL_EXT_NEED_TARGET_SWAP) {
        int tmp;
        tmp = *target_type;
        *target_type = *target_id;
        *target_id = tmp;
    }
}


Bool XNVCTRLQueryExtension (
    Display *dpy,
    int *event_basep,
    int *error_basep
){
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
        if (event_basep) *event_basep = info->codes->first_event;
        if (error_basep) *error_basep = info->codes->first_error;
        return True;
    } else {
        return False;
    }
}


Bool XNVCTRLQueryVersion (
    Display *dpy,
    int *major,
    int *minor
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryExtensionReply rep;
    xnvCtrlQueryExtensionReq   *req;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryExtension, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryExtension;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    if (major) *major = rep.major;
    if (minor) *minor = rep.minor;
    UnlockDisplay (dpy);
    SyncHandle ();
    return True;
}


Bool XNVCTRLIsNvScreen (
    Display *dpy,
    int screen
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlIsNvReply rep;
    xnvCtrlIsNvReq   *req;
    Bool isnv;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlIsNv, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlIsNv;
    req->screen = screen;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    isnv = rep.isnv;
    UnlockDisplay (dpy);
    SyncHandle ();
    return isnv;
}


Bool XNVCTRLQueryTargetCount (
    Display *dpy,
    int target_type,
    int *value
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryTargetCountReply  rep;
    xnvCtrlQueryTargetCountReq   *req;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryTargetCount, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryTargetCount;
    req->target_type = target_type;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    if (value) *value = rep.count;
    UnlockDisplay (dpy);
    SyncHandle ();
    return True;
}


void XNVCTRLSetTargetAttribute (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,
    int value
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSetAttributeReq *req;

    XNVCTRLSimpleCheckExtension (dpy, info);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);

    LockDisplay (dpy);
    GetReq (nvCtrlSetAttribute, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSetAttribute;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;
    req->value = value;
    UnlockDisplay (dpy);
    SyncHandle ();
}

void XNVCTRLSetAttribute (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    int value
){
    XNVCTRLSetTargetAttribute (dpy, NV_CTRL_TARGET_TYPE_X_SCREEN, screen,
                               display_mask, attribute, value);
}


Bool XNVCTRLSetAttributeAndGetStatus (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    int value
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSetAttributeAndGetStatusReq *req;
    xnvCtrlSetAttributeAndGetStatusReply rep;
    Bool success;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlSetAttributeAndGetStatus, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSetAttributeAndGetStatus;
    req->screen = screen;
    req->display_mask = display_mask;
    req->attribute = attribute;
    req->value = value;
    if (!_XReply (dpy, (xReply *) &rep, 0, False)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    UnlockDisplay (dpy);
    SyncHandle ();
    
    success = rep.flags;
    return success;
}



Bool XNVCTRLQueryTargetAttribute (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,
    int *value
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryAttributeReply rep;
    xnvCtrlQueryAttributeReq   *req;
    Bool exists;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryAttribute, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryAttribute;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    if (value) *value = rep.value;
    exists = rep.flags;
    UnlockDisplay (dpy);
    SyncHandle ();
    return exists;
}

Bool XNVCTRLQueryAttribute (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    int *value
){
    return XNVCTRLQueryTargetAttribute(dpy, NV_CTRL_TARGET_TYPE_X_SCREEN,
                                       screen, display_mask, attribute, value);
}


Bool XNVCTRLQueryTargetStringAttribute (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,
    char **ptr
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryStringAttributeReply rep;
    xnvCtrlQueryStringAttributeReq   *req;
    Bool exists;
    int length, numbytes, slop;

    if (!ptr) return False;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryStringAttribute, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryStringAttribute;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;
    if (!_XReply (dpy, (xReply *) &rep, 0, False)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    length = rep.length;
    numbytes = rep.n;
    slop = numbytes & 3;
    *ptr = (char *) Xmalloc(numbytes);
    if (! *ptr) {
        _XEatData(dpy, length);
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    } else {
        _XRead(dpy, (char *) *ptr, numbytes);
        if (slop) _XEatData(dpy, 4-slop);
    }
    exists = rep.flags;
    UnlockDisplay (dpy);
    SyncHandle ();
    return exists;
}

Bool XNVCTRLQueryStringAttribute (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    char **ptr
){
    return XNVCTRLQueryTargetStringAttribute(dpy, NV_CTRL_TARGET_TYPE_X_SCREEN,
                                             screen, display_mask,
                                             attribute, ptr);
}


Bool XNVCTRLSetStringAttribute (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    char *ptr
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSetStringAttributeReq *req;
    xnvCtrlSetStringAttributeReply rep;
    int size;
    Bool success;
    
    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    size = strlen(ptr)+1;

    LockDisplay (dpy);
    GetReq (nvCtrlSetStringAttribute, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSetStringAttribute;
    req->screen = screen;
    req->display_mask = display_mask;
    req->attribute = attribute;
    req->length += ((size + 3) & ~3) >> 2;
    req->num_bytes = size;
    Data(dpy, ptr, size);
    
    if (!_XReply (dpy, (xReply *) &rep, 0, False)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    UnlockDisplay (dpy);
    SyncHandle ();
    
    success = rep.flags;
    return success;
}


Bool XNVCTRLQueryValidTargetAttributeValues (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,                                 
    NVCTRLAttributeValidValuesRec *values
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryValidAttributeValuesReply rep;
    xnvCtrlQueryValidAttributeValuesReq   *req;
    Bool exists;
    
    if (!values) return False;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryValidAttributeValues, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryValidAttributeValues;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    exists = rep.flags;
    values->type = rep.attr_type;
    if (rep.attr_type == ATTRIBUTE_TYPE_RANGE) {
        values->u.range.min = rep.min;
        values->u.range.max = rep.max;
    }
    if (rep.attr_type == ATTRIBUTE_TYPE_INT_BITS) {
        values->u.bits.ints = rep.bits;
    }
    values->permissions = rep.perms;
    UnlockDisplay (dpy);
    SyncHandle ();
    return exists;
}

Bool XNVCTRLQueryValidAttributeValues (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,                                 
    NVCTRLAttributeValidValuesRec *values
){
    return XNVCTRLQueryValidTargetAttributeValues(dpy,
                                                  NV_CTRL_TARGET_TYPE_X_SCREEN,
                                                  screen, display_mask,
                                                  attribute, values);
}


void XNVCTRLSetGvoColorConversion (
    Display *dpy,
    int screen,
    float colorMatrix[3][3],
    float colorOffset[3],
    float colorScale[3]
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSetGvoColorConversionReq *req;

    XNVCTRLSimpleCheckExtension (dpy, info);

    LockDisplay (dpy);
    GetReq (nvCtrlSetGvoColorConversion, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSetGvoColorConversion;
    req->screen = screen;

    req->cscMatrix_y_r = colorMatrix[0][0];
    req->cscMatrix_y_g = colorMatrix[0][1];
    req->cscMatrix_y_b = colorMatrix[0][2];

    req->cscMatrix_cr_r = colorMatrix[1][0];
    req->cscMatrix_cr_g = colorMatrix[1][1];
    req->cscMatrix_cr_b = colorMatrix[1][2];

    req->cscMatrix_cb_r = colorMatrix[2][0];
    req->cscMatrix_cb_g = colorMatrix[2][1];
    req->cscMatrix_cb_b = colorMatrix[2][2];

    req->cscOffset_y  = colorOffset[0];
    req->cscOffset_cr = colorOffset[1];
    req->cscOffset_cb = colorOffset[2];

    req->cscScale_y  = colorScale[0];
    req->cscScale_cr = colorScale[1];
    req->cscScale_cb = colorScale[2];

    UnlockDisplay (dpy);
    SyncHandle ();
}


Bool XNVCTRLQueryGvoColorConversion (
    Display *dpy,
    int screen,
    float colorMatrix[3][3],
    float colorOffset[3],
    float colorScale[3]
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryGvoColorConversionReply rep;
    xnvCtrlQueryGvoColorConversionReq *req;
    
    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);

    GetReq (nvCtrlQueryGvoColorConversion, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryGvoColorConversion;
    req->screen = screen;

    if (!_XReply(dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }

    _XRead(dpy, (char *)(colorMatrix), 36);
    _XRead(dpy, (char *)(colorOffset), 12);
    _XRead(dpy, (char *)(colorScale), 12);

    UnlockDisplay (dpy);
    SyncHandle ();

    return True;
}


Bool XNVCtrlSelectTargetNotify (
    Display *dpy,
    int target_type,
    int target_id,
    int notify_type,
    Bool onoff
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSelectTargetNotifyReq *req;

    if(!XextHasExtension (info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlSelectTargetNotify, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSelectTargetNotify;
    req->target_type = target_type;
    req->target_id = target_id;
    req->notifyType = notify_type;
    req->onoff = onoff;
    UnlockDisplay (dpy);
    SyncHandle ();

    return True;
}


Bool XNVCtrlSelectNotify (
    Display *dpy,
    int screen,
    int type,
    Bool onoff
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlSelectNotifyReq *req;

    if(!XextHasExtension (info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);

    LockDisplay (dpy);
    GetReq (nvCtrlSelectNotify, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlSelectNotify;
    req->screen = screen;
    req->notifyType = type;
    req->onoff = onoff;
    UnlockDisplay (dpy);
    SyncHandle ();

    return True;
}

Bool XNVCTRLQueryTargetBinaryData (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,
    unsigned char **ptr,
    int *len
){
    XExtDisplayInfo *info = find_display (dpy);
    xnvCtrlQueryBinaryDataReply rep;
    xnvCtrlQueryBinaryDataReq   *req;
    Bool exists;
    int length, numbytes, slop;

    if (!ptr) return False;

    if(!XextHasExtension(info))
        return False;

    XNVCTRLCheckExtension (dpy, info, False);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);

    LockDisplay (dpy);
    GetReq (nvCtrlQueryBinaryData, req);
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlQueryBinaryData;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;
    if (!_XReply (dpy, (xReply *) &rep, 0, False)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    }
    length = rep.length;
    numbytes = rep.n;
    slop = numbytes & 3;
    *ptr = (char *) Xmalloc(numbytes);
    if (! *ptr) {
        _XEatData(dpy, length);
        UnlockDisplay (dpy);
        SyncHandle ();
        return False;
    } else {
        _XRead(dpy, (char *) *ptr, numbytes);
        if (slop) _XEatData(dpy, 4-slop);
    }
    exists = rep.flags;
    if (len) *len = numbytes;
    UnlockDisplay (dpy);
    SyncHandle ();
    return exists;
}

Bool XNVCTRLQueryBinaryData (
    Display *dpy,
    int screen,
    unsigned int display_mask,
    unsigned int attribute,
    unsigned char **ptr,
    int *len
){
    return XNVCTRLQueryTargetBinaryData(dpy, NV_CTRL_TARGET_TYPE_X_SCREEN,
                                        screen, display_mask,
                                        attribute, ptr, len);
}

Bool XNVCTRLStringOperation (
    Display *dpy,
    int target_type,
    int target_id,
    unsigned int display_mask,
    unsigned int attribute,
    char *pIn,
    char **ppOut
) {
    XExtDisplayInfo *info = find_display(dpy);
    xnvCtrlStringOperationReq *req;
    xnvCtrlStringOperationReply rep;
    Bool ret;
    int inSize, outSize, length, slop;

    if (!XextHasExtension(info))
        return False;
    
    if (!ppOut)
        return False;

    *ppOut = NULL;
    
    XNVCTRLCheckExtension(dpy, info, False);
    XNVCTRLCheckTargetData(dpy, info, &target_type, &target_id);
    
    if (pIn) {
        inSize = strlen(pIn) + 1;
    } else {
        inSize = 0;
    }
    
    LockDisplay(dpy);
    GetReq(nvCtrlStringOperation, req);
    
    req->reqType = info->codes->major_opcode;
    req->nvReqType = X_nvCtrlStringOperation;
    req->target_type = target_type;
    req->target_id = target_id;
    req->display_mask = display_mask;
    req->attribute = attribute;

    req->length += ((inSize + 3) & ~3) >> 2;
    req->num_bytes = inSize;
    
    if (pIn) {
        Data(dpy, pIn, inSize);
    }
    
    if (!_XReply (dpy, (xReply *) &rep, 0, False)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    
    length = rep.length;
    outSize = rep.num_bytes;
    slop = outSize & 3;

    if (outSize) *ppOut = (char *) Xmalloc(outSize);
    
    if (!*ppOut) {
        _XEatData(dpy, length);
    } else {
        _XRead(dpy, (char *) *ppOut, outSize);
        if (slop) _XEatData(dpy, 4-slop);
    }
    
    ret = rep.ret;
    
    UnlockDisplay(dpy);
    SyncHandle();
    
    return ret;
}


static Bool wire_to_event (Display *dpy, XEvent *host, xEvent *wire)
{
    XExtDisplayInfo *info = find_display (dpy);
    XNVCtrlEvent *re;
    xnvctrlEvent *event;
    XNVCtrlEventTarget *reTarget;
    xnvctrlEventTarget *eventTarget;
    XNVCtrlEventTargetAvailability *reTargetAvailability;
    XNVCtrlStringEventTarget *reTargetString;
    XNVCtrlBinaryEventTarget *reTargetBinary;

    XNVCTRLCheckExtension (dpy, info, False);
    
    switch ((wire->u.u.type & 0x7F) - info->codes->first_event) {
    case ATTRIBUTE_CHANGED_EVENT:
        re = (XNVCtrlEvent *) host;
        event = (xnvctrlEvent *) wire;
        re->attribute_changed.type = event->u.u.type & 0x7F;
        re->attribute_changed.serial =
            _XSetLastRequestRead(dpy, (xGenericReply*) event);
        re->attribute_changed.send_event = ((event->u.u.type & 0x80) != 0);
        re->attribute_changed.display = dpy;
        re->attribute_changed.time = event->u.attribute_changed.time;
        re->attribute_changed.screen = event->u.attribute_changed.screen;
        re->attribute_changed.display_mask =
            event->u.attribute_changed.display_mask;
        re->attribute_changed.attribute = event->u.attribute_changed.attribute;
        re->attribute_changed.value = event->u.attribute_changed.value;
        break;
    case TARGET_ATTRIBUTE_CHANGED_EVENT:
        reTarget = (XNVCtrlEventTarget *) host;
        eventTarget = (xnvctrlEventTarget *) wire;
        reTarget->attribute_changed.type = eventTarget->u.u.type & 0x7F;
        reTarget->attribute_changed.serial =
            _XSetLastRequestRead(dpy, (xGenericReply*) eventTarget);
        reTarget->attribute_changed.send_event =
            ((eventTarget->u.u.type & 0x80) != 0);
        reTarget->attribute_changed.display = dpy;
        reTarget->attribute_changed.time =
            eventTarget->u.attribute_changed.time;
        reTarget->attribute_changed.target_type =
            eventTarget->u.attribute_changed.target_type;
        reTarget->attribute_changed.target_id =
            eventTarget->u.attribute_changed.target_id;
        reTarget->attribute_changed.display_mask =
            eventTarget->u.attribute_changed.display_mask;
        reTarget->attribute_changed.attribute =
            eventTarget->u.attribute_changed.attribute;
        reTarget->attribute_changed.value =
            eventTarget->u.attribute_changed.value;
        break;
    case TARGET_ATTRIBUTE_AVAILABILITY_CHANGED_EVENT:
        reTargetAvailability = (XNVCtrlEventTargetAvailability *) host;
        eventTarget = (xnvctrlEventTarget *) wire;
        reTargetAvailability->attribute_changed.type =
            eventTarget->u.u.type & 0x7F;
        reTargetAvailability->attribute_changed.serial =
            _XSetLastRequestRead(dpy, (xGenericReply*) eventTarget);
        reTargetAvailability->attribute_changed.send_event =
            ((eventTarget->u.u.type & 0x80) != 0);
        reTargetAvailability->attribute_changed.display = dpy;
        reTargetAvailability->attribute_changed.time =
            eventTarget->u.availability_changed.time;
        reTargetAvailability->attribute_changed.target_type =
            eventTarget->u.availability_changed.target_type;
        reTargetAvailability->attribute_changed.target_id =
            eventTarget->u.availability_changed.target_id;
        reTargetAvailability->attribute_changed.display_mask =
            eventTarget->u.availability_changed.display_mask;
        reTargetAvailability->attribute_changed.attribute =
            eventTarget->u.availability_changed.attribute;
        reTargetAvailability->attribute_changed.availability =
            eventTarget->u.availability_changed.availability;
        reTargetAvailability->attribute_changed.value =
            eventTarget->u.availability_changed.value;
        break;
    case TARGET_STRING_ATTRIBUTE_CHANGED_EVENT:
        reTargetString = (XNVCtrlStringEventTarget *) host;
        eventTarget = (xnvctrlEventTarget *) wire;
        reTargetString->attribute_changed.type = eventTarget->u.u.type & 0x7F;
        reTargetString->attribute_changed.serial =
            _XSetLastRequestRead(dpy, (xGenericReply*) eventTarget);
        reTargetString->attribute_changed.send_event =
            ((eventTarget->u.u.type & 0x80) != 0);
        reTargetString->attribute_changed.display = dpy;
        reTargetString->attribute_changed.time =
            eventTarget->u.attribute_changed.time;
        reTargetString->attribute_changed.target_type =
            eventTarget->u.attribute_changed.target_type;
        reTargetString->attribute_changed.target_id =
            eventTarget->u.attribute_changed.target_id;
        reTargetString->attribute_changed.display_mask =
            eventTarget->u.attribute_changed.display_mask;
        reTargetString->attribute_changed.attribute =
            eventTarget->u.attribute_changed.attribute;
        break;
    case TARGET_BINARY_ATTRIBUTE_CHANGED_EVENT:
        reTargetBinary = (XNVCtrlBinaryEventTarget *) host;
        eventTarget = (xnvctrlEventTarget *) wire;
        reTargetBinary->attribute_changed.type = eventTarget->u.u.type & 0x7F;
        reTargetBinary->attribute_changed.serial =
            _XSetLastRequestRead(dpy, (xGenericReply*) eventTarget);
        reTargetBinary->attribute_changed.send_event =
            ((eventTarget->u.u.type & 0x80) != 0);
        reTargetBinary->attribute_changed.display = dpy;
        reTargetBinary->attribute_changed.time =
            eventTarget->u.attribute_changed.time;
        reTargetBinary->attribute_changed.target_type =
            eventTarget->u.attribute_changed.target_type;
        reTargetBinary->attribute_changed.target_id =
            eventTarget->u.attribute_changed.target_id;
        reTargetBinary->attribute_changed.display_mask =
            eventTarget->u.attribute_changed.display_mask;
        reTargetBinary->attribute_changed.attribute =
            eventTarget->u.attribute_changed.attribute;
        break;

    default:
        return False;
    }
    
    return True;
}

