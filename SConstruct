import os
import sys

prog_name = 'klot'

src=Split("main.c solver.c mem.c index.c queue.c board.c state.c base.c list.c")
#~ libsrc=Split("base.c list.c")
#~ libdir = "../library/"

pos_inc = []#[ '/usr/include/freetype2']
pos_libdir = []
pos_libs = ['m', 'pthread' ]

win_inc = []
win_libdir = []
win_libs = []#['m', 'opengl32', 'SDL', 'png',  'zdll', 'freetype', 'user32']

defines = []

# Don't mess with anything below this point
###########################

allsrc = src# + [libdir+file for file in libsrc]
pos_inc = pos_inc# + [libdir]
win_inc = win_inc #+ [libdir]

debug = ARGUMENTS.get('debug', 0)
opt_lev = ARGUMENTS.get('opt', 0)
profile = ARGUMENTS.get('profile', 0)
log_lev = 6

#Global environment
g_env = Environment(ENV = os.environ, CCFLAGS=['-Wall'])
if debug or profile:
	if profile:
	    g_env.Append(CCFLAGS=['-pg'])
	    g_env.Append(LINKFLAGS=['-pg'])
	g_env.Append(CCFLAGS=['-g'])
	log_lev = debug
if opt_lev:
	g_env.Append(CCFLAGS=['-O%s'%(opt_lev)])
g_env.Append(CPPDEFINES=defines)
g_env.Append(CPPDEFINES=['LOG_LEVEL=%s'%log_lev])

#windows environment
win_env = g_env.Clone(CPPPATH=win_inc, LIBS=win_libs, LIBPATH=pos_libdir)# + [libdir])

if os.name == "posix":
	#linux environment
	pos_env = g_env.Clone(CPPPATH=pos_inc, LIBS=pos_libs, LIBPATH=pos_libdir)# + [libdir])
	posexe = pos_env.Program(target = prog_name, source = allsrc)
	
	#windows cross environment	
	win_env.Replace(CC='i386-mingw32msvc-gcc')
	win_objs = [win_env.Object('win32_'+os.path.splitext(file)[0], file) for file in allsrc]
	winexe = win_env.Program(target = prog_name+'.exe', source = win_objs)

	Alias('win32', winexe)
	Alias('all', [posexe, winexe])
	Default([posexe])
elif os.name == 'win32' or os.name == 'nt':
	winexe = win_env.Program(target = prog_name, source = allsrc)
	Default([winexe])
else:
	print "unknwon os " + os.name
	
