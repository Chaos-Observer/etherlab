function setup()

disp(['Precompiling functions in ' pwd]);
mex taskinfo.c
return
