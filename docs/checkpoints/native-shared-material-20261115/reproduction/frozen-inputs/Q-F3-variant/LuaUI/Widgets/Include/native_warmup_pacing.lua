-- Private benchmark guard. No renderer, scene, speed, or counted-anchor changes.
-- GamePaused is the server round-trip acknowledgement; GetGameSpeed alone can
-- reflect the client's optimistic pause request and is not sufficient.
return function(api)
 local state='waiting'
 local warm,ackFrame,requestedAt,ackTime,ackDraw
 local function draw()
  local lo,hi=api.GetDrawFrame();return lo+65536*(hi or 0)
 end
 local function elapsed()
  return warm and api.DiffTimers(api.GetTimer(),warm) or 0
 end
 local function log(event,extra)
  api.Echo(string.format('[WarmupPacing] event=%s simulation=%d wall_seconds=%.9f state=%s%s',event,api.GetGameFrame(),elapsed(),state,extra or ''))
 end
 local function fail(reason)
  if state~='failed' then
   state='failed';log('FAILED_'..reason);api.Quit()
  end
  return false
 end
 local function speedOK()
  local user,actual=api.GetGameSpeed()
  return user==1 and actual==1
 end
 local guard={}
 function guard:Frame(frame,originalWarmTimer)
  if state=='failed' then return false end
  if frame==1 then
   if warm or not originalWarmTimer then return fail('START_TIMER') end
   warm=originalWarmTimer;log('start')
  end
  if frame==180 then
   if state~='waiting' or not warm or not speedOK() then return fail('PAUSE_REQUEST_STATE') end
   local _,_,paused=api.GetGameSpeed()
   if paused then return fail('UNEXPECTED_PRIOR_PAUSE') end
   state='pause_requested';requestedAt=api.GetTimer();log('pause_request')
   api.SendCommands('pause 1')
  end
  if state=='held' and frame~=ackFrame then return fail('ADVANCED_WHILE_ACKNOWLEDGED_PAUSED') end
  if frame>=240 and frame<300 and state~='resumed' then return fail('ACK_DEADLINE_BEFORE_COUNT') end
  if frame==300 then
   local _,_,paused=api.GetGameSpeed()
   if state~='resumed' or paused or not speedOK() or elapsed()<10 then return fail('COUNT_ENTRY') end
   state='counted';log('count_start')
  end
  if frame==1500 then
   if state~='counted' then return fail('COUNT_END') end
   state='finished';log('count_end')
  end
  return true
 end
 function guard:Paused(player,paused)
  local frame=api.GetGameFrame()
  if state=='failed' or state=='finished' then return end
  if state=='counted' then
   if paused and frame<1500 then fail('PAUSE_INSIDE_COUNT') end
   return
  end
  if paused then
   if state~='pause_requested' or frame<180 or frame>=240 then fail('UNEXPECTED_PAUSE_ACK');return end
   state='held';ackFrame=frame;ackTime=api.GetTimer();ackDraw=draw();log('pause_ack')
  else
   if state~='resume_requested' or frame~=ackFrame then fail('UNEXPECTED_RESUME_ACK');return end
   state='resumed';log('resume_ack')
  end
 end
 function guard:Update()
  if state=='failed' then return false end
  if state=='held' then
   local _,_,paused=api.GetGameSpeed()
   if api.GetGameFrame()~=ackFrame or not paused or not speedOK() then return fail('HELD_STATE') end
   if elapsed()>=10 then
    local held=api.DiffTimers(api.GetTimer(),ackTime);local draws=draw()-ackDraw
    if held>0.5 and draws<2 then return fail('RENDER_NOT_ADVANCING') end
    state='resume_requested';requestedAt=api.GetTimer()
    log('resume_request',string.format(' held_seconds=%.9f held_draws=%d',held,draws))
    api.SendCommands('pause 0')
   end
  elseif state=='pause_requested' or state=='resume_requested' then
   if api.DiffTimers(api.GetTimer(),requestedAt)>5 then return fail('ACK_WALL_DEADLINE') end
  end
  return true
 end
 function guard:Ready()return state=='resumed' end
 return guard
end
