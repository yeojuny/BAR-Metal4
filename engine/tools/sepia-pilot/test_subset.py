#!/usr/bin/env python3
"""Bounded offline subset acceptance/changes/rejection tests; no engine/device."""
import copy, hashlib, json, tempfile, unittest
from pathlib import Path
from compile_subset import Parser, compile_sources, fnv, header
HERE=Path(__file__).resolve().parent
VERTEX=(HERE/'sepia.vert.glsl').read_text()
FRAGMENT=(HERE/'sepia.frag.glsl').read_text()
class SubsetTests(unittest.TestCase):
 def test_exact_current_stages(self):
  self.assertEqual(fnv(VERTEX),16905188423406352921)
  self.assertEqual(fnv(FRAGMENT),17382421314025316268)
  msl,m=compile_sources(VERTEX,FRAGMENT)
  self.assertEqual(m['uniformBytes'],128)
  self.assertIn('pow((pilotFetch(',msl)
  self.assertEqual(compile_sources(VERTEX,FRAGMENT),(msl,m))
 def test_equation_exposure_bias_and_layout_are_generated(self):
  changed=FRAGMENT.replace('uniform float gamma;','uniform vec3 bias = vec3(0.1, 0.2, 0.3);\nuniform float exposure = 1.25;\nuniform float gamma;')
  changed=changed.replace('vec3 color = pow(', 'vec3 color = pow(').replace('vec3(gamma));','vec3(gamma)) * exposure + bias;')
  msl,m=compile_sources(VERTEX,changed)
  self.assertIn('u.exposure.x',msl);self.assertIn('u.bias.xyz',msl)
  fields={f['name']:f for f in m['fields']}
  self.assertEqual(fields['bias']['defaults'],[0.1,0.2,0.3]);self.assertEqual(fields['exposure']['defaults'],[1.25])
  self.assertEqual(fields['gamma']['offset'],96);self.assertEqual(m['uniformBytes'],160)
  self.assertNotEqual(m['identity'],compile_sources(VERTEX,FRAGMENT)[1]['identity'])
  self.assertIn('const float value=1.25f',header(VERTEX,changed,msl,m))
 def test_supported_layout_reorder(self):
  changed=FRAGMENT.replace('uniform float gamma;','').replace('uniform vec3 colorB;','uniform vec3 colorB;\nuniform float gamma = 1.0;')
  _,m=compile_sources(VERTEX,changed)
  self.assertEqual(m['fields'][-1]['name'],'gamma');self.assertEqual(m['fields'][-1]['offset'],112)
 def test_fail_closed(self):
  cases=[FRAGMENT.replace('uniform float gamma;','uniform mat4 gamma;'),
   FRAGMENT.replace('vec3 color =','for (int i=0; i<3; ++i) {} vec3 color ='),
   FRAGMENT.replace('viewPos, 0','viewPos, 1'),
   FRAGMENT.replace('ivec2(gl_FragCoord.xy) - viewPos','ivec2(gl_FragCoord.xy) - viewPos + ivec2(1)'),
   FRAGMENT.replace('dot(colorR, color)','unknown(colorR, color)'),
   FRAGMENT.replace('uniform float gamma;','uniform float gamma = 1e40;'),
   FRAGMENT.replace('uniform float gamma;','uniform float gamma[2];'),
   FRAGMENT.replace('uniform float gamma;','uniform float target;\nuniform float gamma;'),
   FRAGMENT+'\nvoid other() {}',FRAGMENT.replace('#version 330','#define SOMETHING 1')]
  for source in cases:
   with self.subTest(source=source):
    with self.assertRaises((ValueError,KeyError,StopIteration)):compile_sources(VERTEX,source)
  with self.assertRaises(ValueError):compile_sources(VERTEX.replace('0.0, 1.0','0.5, 1.0'),FRAGMENT)
if __name__=='__main__':unittest.main()
