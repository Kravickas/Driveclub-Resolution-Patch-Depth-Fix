# Does the depth fix actually run?
#
# Open a capture in RenderDoc, then Window > Python Shell, then:
#
#     exec(open(r'C:\path\to\check_depth_fix.py').read())
#
# It walks every draw in the capture, finds the ones that write depth, reads
# their pixel shader, and says whether the old 960 by 540 is still in there.
# Nothing to hunt for.
#
# What it looks for: before the fix the shader carries 960 and 540 as plain
# numbers. After it, those are gone and the size is taken from the picture the
# shader is drawing into, which turns up as a query or as a descriptor read.
#
# Written against the RenderDoc Python API but never run, because there is no
# RenderDoc here. If a call has moved, the error will say which one.

import renderdoc as rd


def _tex_size(ctrl, rid):
    if rid == rd.ResourceId.Null():
        return None
    for t in ctrl.GetTextures():
        if t.resourceId == rid:
            return (t.width, t.height)
    return None


def _every_action(root):
    for a in root:
        yield a
        for k in _every_action(a.children):
            yield k


def check(ctrl):
    looked = 0
    depth_draws = 0
    stale = []
    fixed = []
    sizes = set()

    for act in _every_action(ctrl.GetRootActions()):
        if not (act.flags & rd.ActionFlags.Drawcall):
            continue
        looked += 1
        ctrl.SetFrameEvent(act.eventId, True)
        state = ctrl.GetPipelineState()

        # only the draws that write depth are of interest
        dsv = state.GetDepthTarget()
        rid = dsv.resource if hasattr(dsv, 'resource') else dsv.resourceId
        size = _tex_size(ctrl, rid)
        if size is None:
            continue
        depth_draws += 1
        sizes.add(size)

        refl = state.GetShaderReflection(rd.ShaderStage.Pixel)
        if refl is None:
            continue
        try:
            pipe = state.GetGraphicsPipelineObject()
            text = ctrl.DisassembleShader(pipe, refl, "")
        except Exception:
            continue

        has_old = ('960' in text and '540' in text)
        asks_size = ('OpImageQuerySize' in text or 'ImageQuery' in text
                     or 's_bfe' in text)
        if has_old and not asks_size:
            stale.append((act.eventId, size))
        elif asks_size:
            fixed.append((act.eventId, size))

    print('')
    print('  looked at %d draw(s); %d of them write depth' % (looked, depth_draws))
    if sizes:
        print('  the depth picture is %s' % ', '.join('%d x %d' % s for s in sorted(sizes)))
    print('')
    if stale:
        print('  NOT FIXED. %d draw(s) still carry 960 and 540:' % len(stale))
        for eid, s in stale[:8]:
            print('     event %d, drawing into %d x %d' % (eid, s[0], s[1]))
        print('')
        print('  run the patcher on your game patch folder, then delete the')
        print('  shader cache, then take the capture again.')
    elif fixed:
        print('  FIXED. %d depth draw(s) take the size from the picture.' % len(fixed))
        for eid, s in fixed[:8]:
            print('     event %d, drawing into %d x %d' % (eid, s[0], s[1]))
    else:
        print('  no depth draw carried either the old numbers or a size query.')
        print('  either this capture has no depth pass in it, or the shader')
        print('  does it in a way this does not recognise.')


pyrenderdoc.Replay().BlockInvoke(check)
