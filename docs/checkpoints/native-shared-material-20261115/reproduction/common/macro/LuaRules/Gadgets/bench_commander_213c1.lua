function gadget:GetInfo()
    return {name="Commander control 213c1", desc="Fixed solo construction sequence", author="Codex", layer=1000, enabled=true}
end
if not gadgetHandler:IsSyncedCode() then return end

local plans = {
    {frame=150,  name="armmex",   x=5184, z=736},
    {frame=370,  name="armsolar", x=5224, z=616},
    {frame=660,  name="armwin",   x=5048, z=616},
    {frame=850,  name="armrad",   x=5040, z=720},
    {frame=1010, name="armvp",    x=5120, z=528},
}
local commander, events, issued, accepted, created, finished = nil, {}, 0, 0, 0, 0
local units = {}
local function note(kind, ...)
    local fields = {kind, Spring.GetGameFrame(), ...}
    for i,value in ipairs(fields) do
        fields[i] = type(value)=="number" and string.format("%.9f",value) or tostring(value)
    end
    events[#events+1] = table.concat(fields,",")
end
local function fail(reason)
    Spring.SetGameRulesParam("macro213_error",reason)
    for _,row in ipairs(events) do Spring.Echo("[Commander213Partial] "..row) end
    error("Commander 213c1: "..reason)
end
local function check(value, reason) if not value then fail(reason) end end
local function resources(kind)
    local m,ms=Spring.GetTeamResources(0,"metal")
    local e,es=Spring.GetTeamResources(0,"energy")
    note(kind,m,ms,e,es)
    return m,e
end
function gadget:GameFrame(frame)
    if frame==90 then
        local teams=Spring.GetTeamList()
        check(#teams==2 and Spring.GetGaiaTeamID()~=0 and #Spring.GetPlayerList()==1,"team composition")
        check(not select(4,Spring.GetTeamInfo(0,false)) and (Spring.GetTeamLuaAI(0) or "")=="","AI team")
        local own=Spring.GetTeamUnits(0)
        check(#own==1 and Spring.GetUnitDefID(own[1])==UnitDefNames.armcom.id,"expected original Armada commander")
        commander=own[1]
        local x,y,z=Spring.GetUnitPosition(commander)
        check(math.abs(x-5120)<1 and math.abs(z-640)<1,"commander spawn")
        Spring.SetTeamResource(0,"ms",2400)
        Spring.SetTeamResource(0,"es",6000)
        Spring.SetTeamResource(0,"metal",1800)
        Spring.SetTeamResource(0,"energy",4500)
        local m,e=resources("initial")
        check(math.abs(m-1800)<0.01 and math.abs(e-4500)<0.01,"initial resource storage")
        note("commander",commander,Spring.GetUnitDefID(commander),x,y,z)
    end
    for i,p in ipairs(plans) do
        if frame==p.frame then
            check(commander and accepted==i-1 and issued==i-1,"previous order missing")
            local def=UnitDefNames[p.name].id
            local y=Spring.GetGroundHeight(p.x,p.z)
            local bx,by,bz=Spring.Pos2BuildPos(def,p.x,y,p.z,0)
            local open,feature=Spring.TestBuildOrder(def,bx,by,bz,0)
            check(bx==p.x and bz==p.z and open==2 and feature==nil,"closed or off-grid site "..p.name)
            if i==1 then
                local metal=Spring.GetMetalAmount(math.floor(bx/Game.metalMapSquareSize),math.floor(bz/Game.metalMapSquareSize))
                check(metal and metal>0,"mex on empty metal map")
                note("metal_site",bx,bz,metal)
            end
            issued=i
            resources("before"..i)
            note("issue",i,p.name,def,bx,by,bz,0)
            check(Spring.GiveOrderToUnit(commander,-def,{bx,by,bz,0},i==1 and {} or {"shift"}),"order rejected")
        end
    end
    if frame==1500 then
        check(issued==5 and accepted==5 and created==5 and finished==4 and #Spring.GetTeamUnits(0)==6,"terminal counts")
        local extraction=Spring.GetUnitMetalExtraction(units[1])
        check(extraction and extraction>0,"mex failed to extract metal")
        note("extraction",units[1],extraction)
        resources("final")
        for i,u in ipairs(units) do
            local x,y,z=Spring.GetUnitPosition(u)
            local _,_,_,_,progress=Spring.GetUnitHealth(u)
            check((i<5 and progress==1 or i==5 and progress>0 and progress<1) and math.abs(x-plans[i].x)<1 and math.abs(z-plans[i].z)<1,"terminal geometry")
            note("built",i,plans[i].name,u,x,y,z,progress)
        end
        Spring.SetGameRulesParam("macro213_rows",#events)
        for i,row in ipairs(events) do Spring.SetGameRulesParam("macro213_row_"..i,row) end
        Spring.SetGameRulesParam("macro213_done",frame)
    end
end
function gadget:UnitCommand(id,def,team,cmd,params,opts)
    if id~=commander or cmd>=0 then return end
    local p=plans[issued]
    check(p and Spring.GetGameFrame()==p.frame and accepted==issued-1 and cmd==-UnitDefNames[p.name].id
        and params[1]==p.x and params[3]==p.z and params[4]==0,"received unexpected build order")
    check((opts.shift and true or false)==(issued>1),"unexpected queue mode")
    accepted=issued
    note("accepted",issued,cmd,params[1],params[2],params[3],params[4],opts.shift and 1 or 0)
end
function gadget:UnitCreated(id,def,team,builder)
    if team~=0 or not commander then return end
    local i=created+1
    local p=plans[i]
    check(p and i<=issued and builder==commander and UnitDefNames[p.name].id==def,"created by wrong owner/order")
    created=i;units[i]=id
    note("created",i,p.name,id,builder)
end
function gadget:UnitFinished(id,def,team)
    if team~=0 or not commander or id==commander then return end
    check(finished+1==created and units[created]==id,"finished unexpected unit")
    finished=created
    note("finished",finished,plans[finished].name,id)
end
