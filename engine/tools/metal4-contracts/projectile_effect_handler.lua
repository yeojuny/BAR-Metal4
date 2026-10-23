local VFSMODE=VFS.ZIP_ONLY
local LOG_SECTION=''
local IsSyncedCode=function()return FixtureSynced end
local isHeadless=false
local headlessDisabledGadgetPrefixes={}
local headlessDisabledCallIns={}
local callInLists={}
local actionHandler={}
local System={}
for k,v in pairs(_G)do System[k]=v end
System.gadget=nil
local gadgetHandler={GG={},knownGadgets={},knownCount=0,orderList={}}
function gadgetHandler:RemoveGadget()error('Unexpected original gadget removal')end
_G.FixtureHandler=gadgetHandler
local function ShouldSkipHeadlessUnsyncedGadget(gadget, basename)
	if not isHeadless or IsSyncedCode() then
		return false
	end

	for i = 1, #headlessDisabledGadgetPrefixes do
		local prefix = headlessDisabledGadgetPrefixes[i]
		if string.sub(basename, 1, #prefix) == prefix then
			return true
		end
	end

	for _, ciName in ipairs(callInLists) do
		if type(gadget[ciName]) == 'function' and not headlessDisabledCallIns[ciName] then
			return false
		end
	end

	return true
end
local function Basename(fullpath)
	local _, _, base = string.find(fullpath, "([^\\/:]*)$")
	local _, _, path = string.find(fullpath, "(.*[\\/:])[^\\/:]*$")
	if (path == nil) then
		path = ""
	end
	return base, path
end












































































































































































































































































































































































































































































































































function gadgetHandler:LoadGadget(filename, overridevfsmode)
	local kbytes = false -- set to number to enable
	if kbytes and collectgarbage then -- only present in special debug builds, otherwise collectgarbage is not preset in synced context!
		collectgarbage("collect") -- call it twice, mark
		collectgarbage("collect") -- sweep
		kbytes = collectgarbage("count")
	end

	local basename = Basename(filename)
	local text = VFS.LoadFile(filename, overridevfsmode or VFSMODE)
	if text == nil then
		Spring.Log(LOG_SECTION, LOG.ERROR, 'Failed to load: ' .. filename)
		return nil
	end
	local chunk, err = loadstring(text, filename)
	if chunk == nil then
		Spring.Log(LOG_SECTION, LOG.ERROR, 'Failed to load: ' .. basename .. '  (' .. err .. ')')
		return nil
	end

	local gadget = gadgetHandler:NewGadget()

	setfenv(chunk, gadget)
	local success, err = pcall(chunk)
	if not success then
		Spring.Log(LOG_SECTION, LOG.ERROR, 'Failed to load: ' .. basename .. '  (' .. err .. ')')
		return nil
	end
	if err == false then -- note that all "normal" gadgets return `nil` implicitly at EOF, so don't do "if not err"
		return nil -- gadget asked for a quiet death
	end

	if gadget.GetInfo and (Platform and not Platform.check(gadget.GetInfo().depends)) then
		Spring.Echo('Missing capabilities:  ' .. gadget:GetInfo().name .. '. Disabling.')
		return nil
	end

	-- raw access to gadgetHandler
	if gadget.GetInfo and gadget:GetInfo().handler then
		gadget.gadgetHandler = self
	end

	self:FinalizeGadget(gadget, filename, basename)
	local name = gadget.ghInfo.name

	err = self:ValidateGadget(gadget)
	if err then
		Spring.Log(LOG_SECTION, LOG.ERROR, 'Failed to load: ' .. basename .. '  (' .. err .. ')')
		return nil
	end

	if ShouldSkipHeadlessUnsyncedGadget(gadget, basename) then
		Spring.Log(LOG_SECTION, LOG.INFO, 'Headless: skipped unsynced gadget: ' .. basename)
		return nil
	end

	local knownInfo = self.knownGadgets[name]
	if knownInfo then
		if knownInfo.active then
			Spring.Log(LOG_SECTION, LOG.ERROR, 'Failed to load: ' .. basename .. '  (duplicate name)')
			return nil
		end
	else
		-- create a knownInfo table
		knownInfo = {}
		knownInfo.desc = gadget.ghInfo.desc
		knownInfo.author = gadget.ghInfo.author
		knownInfo.basename = gadget.ghInfo.basename
		knownInfo.filename = gadget.ghInfo.filename
		self.knownGadgets[name] = knownInfo
		self.knownCount = self.knownCount + 1
		self.knownChanged = true
	end
	knownInfo.active = true

	local info = gadget.GetInfo and gadget:GetInfo()
	local order = self.orderList[name]
	if ((order ~= nil and order > 0) or (order == nil and (info == nil or info.enabled))) then
		-- this will be an active gadget
		if order == nil then
			self.orderList[name] = 12345  -- back of the pack
		else
			self.orderList[name] = order
		end
	else
		self.orderList[name] = 0
		self.knownGadgets[name].active = false
		return nil
	end

	if kbytes then
		collectgarbage("collect") -- mark
		collectgarbage("collect") -- sweep
		Spring.Echo("LoadGadget",filename,"delta=",collectgarbage("count")-kbytes,"total=",collectgarbage("count"),"KB, synced =", IsSyncedCode())
	end
	return gadget
end

function gadgetHandler:NewGadget()
	local gadget = {}
	-- load the system calls into the gadget table
	for k, v in pairs(System) do
		gadget[k] = v
	end
	gadget._G = _G         -- the global table
	gadget.GG = self.GG    -- the shared table
	gadget.gadget = gadget -- easy self referencing

	-- wrapped calls (closures)
	gadget.gadgetHandler = {}
	local gh = gadget.gadgetHandler
	local self = self

	gadget.include = function(f)
		return VFS.Include(f, gadget, VFSMODE)
	end

	gh.RaiseGadget = function(_)
		self:RaiseGadget(gadget)
	end
	gh.LowerGadget = function(_)
		self:LowerGadget(gadget)
	end
	gh.RemoveGadget = function(_)
		self:RemoveGadget(gadget)
	end
	gh.GetViewSizes = function(_)
		return self:GetViewSizes()
	end
	gh.GetHourTimer = function(_)
		return self:GetHourTimer()
	end
	gh.IsSyncedCode = function(_)
		return IsSyncedCode()
	end

	gh.UpdateCallIn = function(_, name)
		self:UpdateGadgetCallIn(name, gadget)
	end
	gh.RemoveCallIn = function(_, name)
		self:RemoveGadgetCallIn(name, gadget)
	end

	gh.RegisterCMDID = function(_, id)
		self:RegisterCMDID(gadget, id)
	end

	gh.RegisterGlobal = function(_, name, value)
		return self:RegisterGlobal(gadget, name, value)
	end
	gh.DeregisterGlobal = function(_, name)
		return self:DeregisterGlobal(gadget, name)
	end
	gh.SetGlobal = function(_, name, value)
		return self:SetGlobal(gadget, name, value)
	end

	gh.AddChatAction = function(_, cmd, func, help)
		return actionHandler.AddChatAction(gadget, cmd, func, help)
	end
	gh.RemoveChatAction = function(_, cmd)
		return actionHandler.RemoveChatAction(gadget, cmd)
	end
	gh.RegisterAllowCommand = function(_, cmdID)
		return self:RegisterAllowCommand(gadget, cmdID)
	end
	gh.DeregisterAllowCommands = function(_)
		return self:DeregisterAllowCommands(gadget)
	end

	if not IsSyncedCode() then
		gh.AddSyncAction = function(_, cmd, func, help)
			return actionHandler.AddSyncAction(gadget, cmd, func, help)
		end
		gh.RemoveSyncAction = function(_, cmd)
			return actionHandler.RemoveSyncAction(gadget, cmd)
		end
	end

	-- for proxied call-ins
	gh.IsMouseOwner = function(_)
		return (self.mouseOwner == gadget)
	end
	gh.DisownMouse = function(_)
		if self.mouseOwner == gadget then
			self.mouseOwner = nil
		end
	end

	return gadget
end

function gadgetHandler:FinalizeGadget(gadget, filename, basename)
	local gi = {}

	gi.filename = filename
	gi.basename = basename
	if gadget.GetInfo == nil then
		gi.name = basename
		gi.layer = 0
	else
		local info = gadget:GetInfo()
		gi.name = info.name or basename
		gi.layer = info.layer or 0
		gi.desc = info.desc or ""
		gi.author = info.author or ""
		gi.license = info.license or ""
		gi.enabled = info.enabled or false
	end

	gadget.ghInfo = {}  --  a proxy table
	local mt = {
		__index = gi,
		__newindex = function()
			error("ghInfo tables are read-only")
		end,
		__metatable = "protected"
	}
	setmetatable(gadget.ghInfo, mt)
	-- cache tracy zone name strings to avoid per-frame string allocation
	if tracy then
		gadget._tracyGameFrameName          = "G:GameFrame:"          .. gi.name
		gadget._tracyGameFramePostName      = "G:GameFramePost:"      .. gi.name
		gadget._tracyViewResizeName         = "G:ViewResize:"         .. gi.name
		gadget._tracyPlayerChangedName      = "G:PlayerChanged:"      .. gi.name
		gadget._tracyUpdateName             = "G:Update:"             .. gi.name
		gadget._tracyDrawWorldName          = "G:DrawWorld:"          .. gi.name
		gadget._tracyDrawWorldPreUnitName   = "G:DrawWorldPreUnit:"   .. gi.name
	end
end

function gadgetHandler:ValidateGadget(gadget)
	if gadget.GetTooltip and not gadget.IsAbove then
		return "Gadget has GetTooltip() but not IsAbove()"
	end
	return nil
end

gadget=gadgetHandler:LoadGadget('LuaRules/Gadgets/gfx_beam_laser_gl4.lua')
FixtureCheck(gadget~=nil,'original ZIP_ONLY LoadGadget reaches scoped native adapter')
