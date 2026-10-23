-- This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
-- Uses the actual loaded official WG.FlowUI.Draw.RectRound implementation.
assert(WG.FlowUI.initialized and WG.FlowUI.version == 1)
assert(WG.FlowUI.vsx == 1920 and WG.FlowUI.vsy == 1080)

WG.FlowUI.Draw.RectRound(10, 20, 110, 80, 10, 1, 1, 1, 1, {0.2, 0.4, 0.6, 0.37})
Fixture.Snapshot("cut_corners")

WG.FlowUI.Draw.RectRound(10, 20, 110, 80, 10, 0, 0, 0, 0, {0.2, 0.4, 0.6, 0.37})
Fixture.Snapshot("square_corners")

gl.Blending(GL.SRC_ALPHA, GL.ONE)
gl.Scissor(20, 25, 50, 40)
WG.FlowUI.Draw.RectRound(10, 20, 110, 80, 10, 1, 0, 1, 0, {1, 0, 0, 0.2}, {0, 0, 1, 0.8})
Fixture.Snapshot("gradient")

-- RectRound has no implicit color/blend/scissor restoration. Missing c1/c2
-- retains the actual final gradient color from the preceding call.
WG.FlowUI.Draw.RectRound(10, 20, 110, 80, 10)
Fixture.Snapshot("inherited_state")

-- The original helper rejects reversed/empty bounds before issuing BeginEnd.
WG.FlowUI.Draw.RectRound(10, 20, 10, 80, 10, 1, 1, 1, 1, {1, 1, 1, 1})
Fixture.Snapshot("invalid_bounds")

gl.Blending(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)
gl.Scissor(false)
WG.FlowUI.Draw.RectRound(0, 0, 10, 10, 0, 1, 1, 1, 1, {1, 1, 1, 0.25})
Fixture.Snapshot("minimum_corner")

-- Unsupported graphics must raise errors, including operations reached inside
-- a callback. Failed callbacks must not leave the recorder inside a batch.
assert(not pcall(function() gl.Texture("not-supported") end))
assert(not pcall(function() gl.BeginEnd(GL.TRIANGLE_FAN, function() end) end))
assert(not pcall(function()
	gl.BeginEnd(GL.QUADS, function()
		gl.Vertex(0, 0, 0)
		gl.Texture("not-supported")
	end)
end))
assert(not pcall(function()
	gl.BeginEnd(GL.QUADS, function() gl.Vertex(0, 0, 0) end)
end))
Fixture.Snapshot("error_recovery")
