# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
"""Focused lexical boundary tests for the static graphics inventory."""
import unittest

from inventory import infer_asset_stages, mask_lua, shader_requirements, source_record


class InventoryBoundaries(unittest.TestCase):
	def test_comments_and_strings_are_not_graphics_code(self):
		source = '''-- gl.BadComment()
local x = "gl.BadString()"
--[=[ gl.BadLongComment() ]=]
local shader = [==[ void main() { gl_Position = vec4(1); } ]==]
gl.Texture(x)
'''
		code, strings, errors = mask_lua(source)
		self.assertIn("gl.Texture(x)", code)
		self.assertNotIn("BadComment", code)
		self.assertNotIn("BadLongComment", code)
		self.assertNotIn("BadString", code)
		self.assertNotIn("gl_Position", code)
		self.assertTrue(any("gl_Position" in payload for _, payload in strings))
		self.assertEqual(code.count("\n"), source.count("\n"))
		self.assertFalse(errors)

	def test_escaped_quote_does_not_leak_string_code(self):
		code, _, errors = mask_lua('local s = "a\\\" gl.Hidden()"\ngl.Color(1)')
		self.assertNotIn("Hidden", code)
		self.assertIn("gl.Color(1)", code)
		self.assertFalse(errors)

	def test_unclosed_long_string_is_reported(self):
		code, _, errors = mask_lua('local shader = [=[\n gl.Hidden()')
		self.assertNotIn("Hidden", code)
		self.assertEqual(len(errors), 1)

	def test_shader_asset_stage_is_not_geometry_for_quad_name(self):
		self.assertEqual(infer_asset_stages("quad.vert.glsl"), {"vertex"})
		self.assertNotIn("family-specific-equivalent-geometry-expansion", shader_requirements("gl_Position = vec4(1);", {"vertex"}))
		self.assertIn("family-specific-equivalent-geometry-expansion", shader_requirements("EmitVertex();", set()))
		self.assertNotIn("family-specific-equivalent-geometry-expansion", shader_requirements("// equivalent to GS EmitVertex()\ngl_Position = vec4(1);", {"vertex"}))

	def test_rectangle_sampling_and_texrect_remain_distinct(self):
		self.assertIn("pixel-coordinate-texture-sampling", shader_requirements("uniform sampler2DRect screen;", {"fragment"}))
		self.assertNotIn("pixel-coordinate-texture-sampling", shader_requirements("gl.TexRect(0, 0, 1, 1)", set()))

	def test_compute_requires_consumer_dependency(self):
		self.assertIn("native-compute-and-consumer-dependency", shader_requirements("gl_GlobalInvocationID.x", set()))

	def test_git_checkout_line_endings_are_explicit(self):
		import hashlib
		canonical = b"gl.Texture(1)\n"
		blob = hashlib.sha1(b"blob " + str(len(canonical)).encode() + b"\0" + canonical).hexdigest()
		record = source_record("bar", "sample.lua", b"gl.Texture(1)\r\n", blob, "checkout")
		self.assertTrue(record["matches_pinned_git_blob"])
		self.assertEqual(record["git_blob_match_mode"], "checkout-crlf-to-lf")
		changed = source_record("bar", "sample.lua", b"gl.Texture(2)\r\n", blob, "checkout")
		self.assertFalse(changed["matches_pinned_git_blob"])


if __name__ == "__main__":
	unittest.main()
