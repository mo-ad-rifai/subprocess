import glob
import subprocess as sp

fails = 0
for testfile in glob.iglob('test*.cpp'):
	print('[Info] Compiling', testfile)
	try:
		p = sp.Popen(['c++', '-std=c++17', '-pthread', '-I..', '-o', 'a.exe', testfile], stderr=sp.STDOUT, universal_newlines=True)
		p.wait(10)
	except Exception as e:
		print('[Error] Compilation of', testfile, 'failed ✘.', str(e))
		p.kill()
		p.communicate()
		fails += 1
		continue
	if p.returncode != 0:
		print('[Error] Compilation of', testfile, 'failed ✘')
		fails += 1
		continue

	print('[Info] Running')
	try:
		p = sp.Popen(['./a.exe'], stdout=sp.PIPE, stderr=sp.PIPE, universal_newlines=True)
		out, err = p.communicate(timeout=10)
	except Exception as e:
		print('[Error] Test', testfile, 'failed ✘.', str(e))
		p.kill()
		p.communicate()
		fails += 1
		continue
	if p.returncode != 0:
		print('[Info] Return code', p.returncode)
		print('[Info] StdOut')
		print(out)
		print('[Info] StdErr')
		print(err)
		print('[Error] Test', testfile, 'failed ✘')
		fails += 1
		continue
	try:
		ref_out = testfile[:-4] + '.ref.out'
		with open(ref_out) as f:
			ref_out = f.read()
		if out != ref_out:
			print('[Info] StdOut')
			print(out)
			print('[Info] StdErr')
			print(err)
			print('[Error] Test', testfile, 'failed ✘')
			fails += 1
			continue
	except FileNotFoundError:
		pass
	try:
		ref_err = testfile[:-4] + '.ref.err'
		with open(ref_err) as f:
			ref_err = f.read()
		if err != ref_err:
			print('[Info] StdOut')
			print(out)
			print('[Info] StdErr')
			print(err)
			print('[Error] Test', testfile, 'failed ✘')
			fails += 1
			continue
	except FileNotFoundError:
		pass

	print('[Info] Test', testfile, 'passed ✓')
exit(fails)
