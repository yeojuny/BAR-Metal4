#! /usr/bin/env python

import sys,os,shutil
import pybindgen
from pybindgen.gccxmlparser import ModuleParser
from pybindgen.typehandlers import base as typehandlers
from pybindgen import ReturnValue, Parameter, Module, Function, FileCodeSink
from pybindgen import CppMethod, CppConstructor, CppClass, Enum
from pygccxml.declarations.class_declaration import class_t
from pygccxml.declarations.calldef import free_function_t, member_function_t, constructor_t, calldef_t

ignored_functions = ['GetMapInfoEx', 'GetMapInfo', 'GetMinimap']

my_parameter_annotations = {
	'ReadFileVFS': { 
		'buf': {'transfer_ownership':'false','direction':'out'}
	},
	'ReadArchiveFile': {
		'buffer': {'transfer_ownership':'false','direction':'out'}
	},
	'GetInfoMapSize': {
		'height': {'transfer_ownership':'false','direction':'out'},
		'width': {'transfer_ownership':'false','direction':'out'}
	},
	'GetInfoMap': {
		'data': {'transfer_ownership':'false','direction':'out'}
	},
	'FindFilesArchive': {
		'size': {'transfer_ownership':'false','direction':'out'}
	}
}

def pre_scan_hook(dummy_module_parser,
                  pygccxml_definition,
                  global_annotations,
                  parameter_annotations):
	## classes
	if isinstance(pygccxml_definition, class_t):
		pass

	## free functions
	if isinstance(pygccxml_definition, free_function_t):
		if pygccxml_definition.name in ignored_functions:
			global_annotations['ignore'] = None
			return
		else:
			try:
				annotations = my_parameter_annotations[pygccxml_definition.name]
			except KeyError:
				pass
			else:
				parameter_annotations.update(annotations)

class BufferReturn(ReturnValue):
	CTYPES = []

	def __init__(self, ctype, length_expression):
		super(BufferReturn, self).__init__(ctype, is_const=False)
		self.length_expression = length_expression

	def convert_c_to_python(self, wrapper):
		pybuf = wrapper.after_call.declare_variable("PyObject*", "pybuf")
		wrapper.after_call.write_code("%s = PyBuffer_FromReadWriteMemory(retval, (%s)*sizeof(unsigned short int));" % (pybuf, self.length_expression))
		wrapper.build_params.add_parameter("N", [pybuf], prepend=True)

def my_module_gen(inputdir,outputdir,includedirs):
	aliases = [ ('uint8_t*', 'unsigned char*')]
	for alias in aliases:
		typehandlers.add_type_alias( alias[0], alias[1] )
	generator_fn = os.path.join( outputdir, 'unitsync_python_wrapper.cc' )
	module_parser = ModuleParser('pyunitsync')
	module_parser.add_pre_scan_hook(pre_scan_hook)
	
	with open(generator_fn,'wb') as output:#ensures file is closed after output
		module = module_parser.parse([os.path.join( inputdir, 'unitsync_api.h')], include_paths=includedirs ,
				includes=['"../unitsync.h"','"../unitsync_api.h"'],)
		module.add_function("GetMinimap", BufferReturn("unsigned short*", "1024*1024"), [Parameter.new('const char*', 'fileName'),Parameter.new('int', 'mipLevel')])
		module.generate( FileCodeSink(output) )

if __name__ == '__main__':
	my_module_gen(sys.argv[1],sys.argv[2],sys.argv[3:])
