"""Use the native Lua renderer readback in the paused frame after the timed anchors."""

from pathlib import Path
import shutil

PROBE = {}


def configure(controller):
    source = controller.read_text()
    old = 'function widget:DrawScreen()\n if not started or frameStopped then return end'
    assert source.count(old) == 1
    source = source.replace(old, '''
function widget:DrawScreen()
 if captureAtDraw and not captureDone then
  local lo,hi=Spring.GetDrawFrame()
  if lo+65536*hi>=captureAtDraw then
   captureDone=true
   local w,h=Spring.GetViewGeometry()
   local ok=gl.SaveImage(0,0,w,h,'LuaUI/Config/h1-scene.png')
   Spring.Echo('[H1NativeImage] success='..tostring(ok)..' simulation='..Spring.GetGameFrame())
   budgetMark('validation-image')
  end
 end
 if not started or frameStopped then return end''')
    old = ' if f==2250 then endMeasurement(f) end'
    assert source.count(old) == 1
    source = source.replace(old, '''
 if f==2250 then
  endMeasurement(f)
  Spring.SendCommands('pause 1')
 end''')
    old = ' if frameStopped and game>=2250 then'
    assert source.count(old) == 1
    source = source.replace(old, '''
 if frameStopped then
  local _,_,paused=Spring.GetGameSpeed()
  if not paused then return end
  if not captureAtDraw then
   captureAtDraw=draw()+8
   Spring.Echo(string.format('[H1ValidationFrame] sim=%d camera=%s',game,camera()))
  end
  if not captureDone then return end
 end
 if frameStopped and game>=2250 then''')
    controller.write_text('local captureAtDraw,captureDone\n' + source)


def capture(data, out, stop, failures):
    marker = data / 'LuaUI/Config/budget192.validation-image'
    while not marker.exists():
        if stop.wait(0.05) and not marker.exists():
            failures.append('The native image marker was absent')
            return
    image = marker.parent / 'h1-scene.png'
    if image.is_file() and image.stat().st_size > 1000:
        shutil.copyfile(image, out / 'scene.png')
    else:
        failures.append('The native readback did not yield a screenshot')
