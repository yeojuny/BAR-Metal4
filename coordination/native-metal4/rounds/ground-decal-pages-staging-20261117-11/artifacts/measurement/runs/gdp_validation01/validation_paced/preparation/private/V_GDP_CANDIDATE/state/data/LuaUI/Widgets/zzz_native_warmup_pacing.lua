function widget:GetInfo()
 return {name='Native Precount Warmup Pacing',desc='Private equal warmup before unchanged 300..1500 benchmark',author='Codex',layer=100001,enabled=true}
end
local make=VFS.Include('LuaUI/Widgets/Include/native_warmup_pacing.lua',nil,VFS.RAW_FIRST)
local guard=make(Spring)
local warm
function widget:GameFrame(f)
 if f==1 then warm=Spring.GetTimer() end
 guard:Frame(f,warm)
end
function widget:Update()guard:Update()end
function widget:GamePaused(player,paused)
 guard:Paused(player,paused)
 if guard:Ready() then
  local user,actual,stillPaused=Spring.GetGameSpeed()
  local f=Spring.GetGameFrame()
  if stillPaused or user~=1 or actual~=1 or f>=240 then
   Spring.Echo('[WarmupPacing] event=FAILED_REMOVE_STATE');Spring.Quit();return
  end
  Spring.Echo(string.format('[WarmupPacing] event=removed_before_count simulation=%d wall_seconds=%.9f',f,Spring.DiffTimers(Spring.GetTimer(),warm)))
  widgetHandler:RemoveWidget(self) -- No guard callback/control work in count300..1500.
 end
end
